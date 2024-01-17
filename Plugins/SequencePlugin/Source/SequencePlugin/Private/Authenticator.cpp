// Fill out your copyright notice in the Description page of Project Settings.

#include "Authenticator.h"
#include "Misc/Guid.h"
#include "AES/aes.c"
#include "AES/aes.h"
#include "Eth/EthTransaction.h"
#include "Eth/Crypto.h"
#include "Types/Wallet.h"
#include "Bitcoin-Cryptography-Library/cpp/Keccak256.hpp"
#include "Bitcoin-Cryptography-Library/cpp/Sha256Hash.hpp"
#include "Bitcoin-Cryptography-Library/cpp/Sha256.hpp"
#include "Util/HexUtility.h"

UAuthenticator::UAuthenticator()
{
	this->Nonce = FGuid::NewGuid().ToString();
	this->StateToken = FGuid::NewGuid().ToString();
}

void UAuthenticator::CallAuthRequiresCode()
{
	if (this->AuthRequiresCode.IsBound())
		this->AuthRequiresCode.Broadcast();
	else
		UE_LOG(LogTemp, Error, TEXT("[System Failure: nothing bound to delegate: AuthRequiresCode]"));
}

void UAuthenticator::CallAuthFailure()
{
	if (this->AuthFailure.IsBound())
		this->AuthFailure.Broadcast();
	else
		UE_LOG(LogTemp, Error, TEXT("[System Error: nothing bound to delegate: AuthFailure]"));
}

void UAuthenticator::CallAuthSuccess(const FCredentials_BE& Credentials)
{
	if (this->AuthSuccess.IsBound())
		this->AuthSuccess.Broadcast(Credentials);
	else
		UE_LOG(LogTemp, Error, TEXT("[System Error: nothing bound to delegate: AuthSuccess]"));
}

void UAuthenticator::SetSocialLoginType(const ESocialSigninType& Type)
{
	this->SocialSigninType = Type;
}

FString UAuthenticator::GetSigninURL()
{
	FString SigninURL = TEXT("");
	
	if (this->SSOProviderMap.Contains(this->SocialSigninType))
	{
		SigninURL = this->GenerateSigninURL(this->SSOProviderMap[this->SocialSigninType].URL, this->SSOProviderMap[this->SocialSigninType].ClientID);
	}
	else
	{
		FString SSOType = UEnum::GetValueAsString(this->SocialSigninType.GetValue());
		UE_LOG(LogTemp, Error, TEXT("No Entry for SSO type: %s in SSOProviderMap"),*SSOType);
	}

	return SigninURL;
}

FString UAuthenticator::GetRedirectURL()
{
	return FString::Printf(TEXT("%s"), *this->RedirectURL);
}

void UAuthenticator::SocialLogin(const FString& IDTokenIn)
{
	this->Cached_IDToken = IDTokenIn;
	CognitoIdentityGetID(this->IdentityPoolID,GetISSClaim(this->Cached_IDToken), this->Cached_IDToken);
}

void UAuthenticator::EmailLogin(const FString& EmailIn)
{
	this->ResetRetryEmailLogin();
	this->Cached_Email = EmailIn;
	CognitoIdentityInitiateAuth(this->Cached_Email,this->CognitoClientID);
}

FString UAuthenticator::GenerateSigninURL(const FString& AuthURL, const FString& ClientID)
{
	return FString::Printf(TEXT("%s?response_type=id_token&client_id=%s&redirect_uri=%s&scope=openid+profile+email&state=%s&nonce=%s"),*AuthURL,*ClientID,*this->RedirectURL,*this->StateToken, *this->Nonce);
}

FString UAuthenticator::BuildAWSURL(const FString& Service, const FString& AWSRegion)
{
	return FString::Printf(TEXT("https://%s.%s.amazonaws.com"),*Service,*AWSRegion);
}

FString UAuthenticator::GetISSClaim(const FString& JWT)
{
	FString ISSClaim = TEXT("");

	TArray<FString> B64Json;
	JWT.ParseIntoArray(B64Json, TEXT("."), true);
	
	for (int i = 0; i < B64Json.Num(); i++)
	{
		FString T;
		FBase64::Decode(B64Json[i], T);
		B64Json[i] = T;
	}

	if (B64Json.Num() > 1)
	{
		TSharedPtr<FJsonObject> json = this->ResponseToJson(B64Json[1]);
		FString iss;
		if (json.Get()->TryGetStringField(TEXT("iss"), iss))
		{
			if (iss.Contains(TEXT("https://"),ESearchCase::IgnoreCase))
				ISSClaim = iss.RightChop(8);
			else
				ISSClaim = iss;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[JWT didn't contain an ISS field]"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[JWT Parse Failure]"));
	}
	return ISSClaim;
}

void UAuthenticator::CognitoIdentityGetID(const FString& PoolID,const FString& Issuer, const FString& IDToken)
{
	FString URL = this->BuildAWSURL(TEXT("cognito-identity"),this->Region);
	FString RequestBody = FString::Printf(TEXT("{\"IdentityPoolId\":\"%s\",\"Logins\":{\"%s\":\"%s\"}}"),*PoolID,*Issuer, *IDToken);
	const TSuccessCallback<FString> GenericSuccess = [this,Issuer](const FString response)
	{
		UE_LOG(LogTemp, Display, TEXT("Response %s"),*response);
		TSharedPtr<FJsonObject> responseObj = this->ResponseToJson(response);
		FString IdentityIdPtr = TEXT("nil");
		if (responseObj->TryGetStringField(TEXT("IdentityId"),IdentityIdPtr))
		{//good state
			this->IdentityId = IdentityIdPtr;
			CognitoIdentityGetCredentialsForIdentity(this->IdentityId, this->Cached_IDToken, Issuer);
		}
		else
		{//error state
			UE_LOG(LogTemp, Error, TEXT("[No IdentityID found in AWS Response]"));
			this->CallAuthFailure();
		}
	};

	const FFailureCallback GenericFailure = [this](const FSequenceError Error)
	{
		UE_LOG(LogTemp, Display, TEXT("[Error: %s]"),*Error.Message);
		this->CallAuthFailure();
	};

	this->RPC(URL,TEXT("com.amazonaws.cognito.identity.model.AWSCognitoIdentityService.GetId"), RequestBody, GenericSuccess, GenericFailure);
}

void UAuthenticator::CognitoIdentityGetCredentialsForIdentity(const FString& IdentityID, const FString& IDToken, const FString& Issuer)
{
	FString URL = this->BuildAWSURL(TEXT("cognito-identity"),this->Region);
	FString RequestBody = FString::Printf(TEXT("{\"IdentityId\":\"%s\",\"Logins\":{\"%s\":\"%s\"}}"), *IdentityID, *Issuer, *IDToken);
							
	const TSuccessCallback<FString> GenericSuccess = [this](const FString response)
	{
		UE_LOG(LogTemp, Display, TEXT("Response %s"), *response);
		TSharedPtr<FJsonObject> responseObj = this->ResponseToJson(response);
		const TSharedPtr<FJsonObject> *credObj;

		if (responseObj->TryGetObjectField(TEXT("Credentials"), credObj))
		{
			FString AccessKeyPtr, SecretKeyPtr, SessionTokenPtr;
			if (credObj->Get()->TryGetStringField(TEXT("AccessKeyId"), AccessKeyPtr) && credObj->Get()->TryGetStringField(TEXT("SecretKey"), SecretKeyPtr) && credObj->Get()->TryGetStringField(TEXT("SessionToken"), SessionTokenPtr))
			{//good state
				this->AccessKeyId = AccessKeyPtr;
				this->SecretKey = SecretKeyPtr;
				this->SessionToken = SessionTokenPtr;
				this->KMSGenerateDataKey(this->KMSKeyID);
			}
			else
			{//error state
				UE_LOG(LogTemp, Error, TEXT("[Missing Credentials in AWS Response]"));
				this->CallAuthFailure();
			}
		}
		else
		{//error state
			UE_LOG(LogTemp, Error, TEXT("[Missing Credentials in AWS Response]"));
			this->CallAuthFailure();
		}
	};

	const FFailureCallback GenericFailure = [this](const FSequenceError Error)
	{
		UE_LOG(LogTemp, Display, TEXT("[Error: %s]"), *Error.Message);
		this->CallAuthFailure();
	};

	this->RPC(URL,TEXT("AWSCognitoIdentityService.GetCredentialsForIdentity"),RequestBody,GenericSuccess,GenericFailure);
}

FString UAuthenticator::BuildYYYYMMDD(const FDateTime& Date)
{
	return Date.ToString(TEXT("%Y%m%d"));//YYYYMMDD
}

FString UAuthenticator::BuildFullDateTime(const FDateTime& Date)
{
	return Date.ToString(TEXT("%Y%m%dT%H%M%SZ"));//YYYYMMDDTHHMMSSZ
}

FString UAuthenticator::BuildScope(const FDateTime& Date)
{
	return FString::Printf(TEXT("%s"), *BuildYYYYMMDD(Date)) + TEXT("/") + FString::Printf(TEXT("%s"), *this->Region) + TEXT("/") + FString::Printf(TEXT("%s"), *this->AWSService) + TEXT("/aws4_request");
}

FString UAuthenticator::BuildCanonicalRequest(const FString& URI, const FDateTime& Date, const FString& Payload)
{
	FUnsizedData PayloadBytes = StringToUTF8(Payload);
	const Sha256Hash PayloadHashBytes = Sha256::getHash(PayloadBytes.Arr, PayloadBytes.GetLength());

	FString Verb = TEXT("POST");
	FString CanonicalURI = TEXT("/");//we have none
	FString CanQueryString = TEXT("");//no query parameters so leave this blank
	FString HashPayload = BytesToHex(PayloadHashBytes.value, PayloadHashBytes.HASH_LEN).ToLower();
	FString CanHeaders = TEXT("content-type:application/x-amz-json-1.1\nhost:") + FString::Printf(TEXT("%s"), *URI) + TEXT("\nx-amz-content-sha256:") + FString::Printf(TEXT("%s"), *HashPayload) + TEXT("\nx-amz-date:") + FString::Printf(TEXT("%s"), *BuildFullDateTime(Date)) + TEXT("\nx-amz-target:TrentService.GenerateDataKey\n");
	FString SignedHeaders = TEXT("content-type;host;x-amz-content-sha256;x-amz-date;x-amz-target");

	//build the CanonicalRequest
	FString CanReq = FString::Printf(TEXT("%s"), *Verb) + TEXT("\n") + FString::Printf(TEXT("%s"), *CanonicalURI) + TEXT("\n") + FString::Printf(TEXT("%s"), *CanQueryString) + TEXT("\n") + FString::Printf(TEXT("%s"), *CanHeaders) + TEXT("\n") + FString::Printf(TEXT("%s"), *SignedHeaders) + TEXT("\n") + FString::Printf(TEXT("%s"), *HashPayload);

	//cleanup
	delete[] PayloadBytes.Arr;
	return CanReq;
}

/*
* 
*/
FString UAuthenticator::BuildStringToSign(const FDateTime& Date, const FString& CanonicalRequest)
{
	FUnsizedData PayloadBytes = StringToUTF8(CanonicalRequest);
	const Sha256Hash PayloadHashBytes = Sha256::getHash(PayloadBytes.Arr, PayloadBytes.GetLength());

	FString CanonicalHash = BytesToHex(PayloadHashBytes.value,PayloadHashBytes.HASH_LEN);
	FString FullDate = BuildFullDateTime(Date);
	FString StringToSign = TEXT("AWS4-HMAC-SHA256\n") + FString::Printf(TEXT("%s"), *FullDate) + TEXT("\n") + FString::Printf(TEXT("%s"), *this->BuildScope(Date)) + TEXT("\n") + FString::Printf(TEXT("%s"), *CanonicalHash.ToLower());

	FUnsizedData StringToSignBytes = StringToUTF8(StringToSign);
	const Sha256Hash StringToSignHashBytes = Sha256::getHash(StringToSignBytes.Arr, StringToSignBytes.GetLength());
	FString StringToSignHash = BytesToHex(StringToSignHashBytes.value, StringToSignHashBytes.HASH_LEN).ToLower();

	//cleanup
	delete[] PayloadBytes.Arr;
	delete[] StringToSignBytes.Arr;

	return StringToSign;
}

TArray<uint8_t> UAuthenticator::BuildSigningKey(const FDateTime& Date)
{
	FUnsizedData DateKeyEncryptBytes = StringToUTF8(TEXT("AWS4") + this->SecretKey);
	FUnsizedData TimeStampBytes = StringToUTF8(BuildYYYYMMDD(Date));
	FUnsizedData RegionBytes = StringToUTF8(this->Region);
	FUnsizedData KMSBytes = StringToUTF8(this->AWSService);
	FUnsizedData AWSReqBytes = StringToUTF8(TEXT("aws4_request"));

	Sha256Hash DateKeyHash = Sha256::getHmac(DateKeyEncryptBytes.Arr,DateKeyEncryptBytes.GetLength(),TimeStampBytes.Arr,TimeStampBytes.GetLength());
	Sha256Hash DateRegionKeyHash = Sha256::getHmac(DateKeyHash.value, DateKeyHash.HASH_LEN, RegionBytes.Arr, RegionBytes.GetLength());
	Sha256Hash DateRegionServiceKeyHash = Sha256::getHmac(DateRegionKeyHash.value, DateRegionKeyHash.HASH_LEN, KMSBytes.Arr, KMSBytes.GetLength());
	Sha256Hash SigningKeyHash = Sha256::getHmac(DateRegionServiceKeyHash.value, DateRegionServiceKeyHash.HASH_LEN, AWSReqBytes.Arr, AWSReqBytes.GetLength());

	FString DateKey = BytesToHex(DateKeyHash.value, DateKeyHash.HASH_LEN).ToLower();
	FString DateRegionKey = BytesToHex(DateRegionKeyHash.value,DateRegionKeyHash.HASH_LEN).ToLower();
	FString DateRegionServiceKey = BytesToHex(DateRegionServiceKeyHash.value, DateRegionServiceKeyHash.HASH_LEN).ToLower();
	FString SigningKey = BytesToHex(SigningKeyHash.value, SigningKeyHash.HASH_LEN).ToLower();

	TArray<uint8_t> SigningBytes;
	SigningBytes.Append(SigningKeyHash.value,SigningKeyHash.HASH_LEN);

	//cleanup
	delete[] DateKeyEncryptBytes.Arr;
	delete[] TimeStampBytes.Arr;
	delete[] RegionBytes.Arr;
	delete[] KMSBytes.Arr;
	delete[] AWSReqBytes.Arr;

	return SigningBytes;
}

FString UAuthenticator::BuildSignature(const TArray<uint8_t>& SigningKey, const FString& StringToSign)
{
	FUnsizedData StringToSignBytes = StringToUTF8(StringToSign);
	Sha256Hash SignatureHash = Sha256::getHmac(SigningKey.GetData(),SigningKey.Num(), StringToSignBytes.Arr, StringToSignBytes.GetLength());
	FString Signature = BytesToHex(SignatureHash.value,SignatureHash.HASH_LEN).ToLower();

	//cleanup
	delete[] StringToSignBytes.Arr;

	return Signature;
}

FString UAuthenticator::BuildKMSAuthorizationHeader(const FDateTime& Date, const FString& URI, const FString& Payload)
{
	FString AuthHeader = TEXT("AWS4-HMAC-SHA256 Credential=");
	FString dateString = this->BuildYYYYMMDD(Date);
	FString CanReq = this->BuildCanonicalRequest(URI, Date, Payload);
	FString StringToSign = this->BuildStringToSign(Date, CanReq);
	TArray<uint8_t> SigningKey = this->BuildSigningKey(Date);
	FString Signature = this->BuildSignature(SigningKey, StringToSign);
	AuthHeader += FString::Printf(TEXT("%s"), *this->AccessKeyId) + TEXT("/") + FString::Printf(TEXT("%s"), *dateString) + TEXT("/") + FString::Printf(TEXT("%s"), *this->Region) + TEXT("/") + FString::Printf(TEXT("%s"), *this->AWSService) + TEXT("/aws4_request,SignedHeaders=content-type;host;x-amz-content-sha256;x-amz-date;x-amz-target,Signature=") + FString::Printf(TEXT("%s"), *Signature);

	return AuthHeader;
}

void UAuthenticator::KMSGenerateDataKey(const FString& AWSKMSKeyID)
{
	FString URL = BuildAWSURL(TEXT("kms"),this->Region);
	FString RequestBody = FString::Printf(TEXT("{\"KeyId\":\"%s\",\"KeySpec\":\"AES_256\"}"), *AWSKMSKeyID);

	const TSuccessCallback<FString> GenericSuccess = [this](const FString response)
	{
		UE_LOG(LogTemp, Display, TEXT("Response %s"), *response);
		TSharedPtr<FJsonObject> responseObj = this->ResponseToJson(response);
		FString PlainTextPtr, CipherTextBlobPtr;
		if (responseObj->TryGetStringField(TEXT("Plaintext"), PlainTextPtr) && responseObj->TryGetStringField(TEXT("CipherTextBlob"), CipherTextBlobPtr))
		{//good state
			TArray<uint8> PlainTextBytes;
			FBase64::Decode(PlainTextPtr, PlainTextBytes);
			this->PlainText = BytesToHex(PlainTextBytes.GetData(),PlainTextBytes.Num()).ToLower();

			TArray<uint8> CipherTextBytes;
			FBase64::Decode(CipherTextBlobPtr, CipherTextBytes);
			this->CipherTextBlob = BytesToHex(CipherTextBytes.GetData(), CipherTextBytes.Num()).ToLower();
			this->AuthWithSequence(this->Cached_IDToken,PlainTextBytes);
		}
		else
		{//error state
			UE_LOG(LogTemp, Error, TEXT("[Missing Credentials in AWS Response]"));
			this->CallAuthFailure();
		}
	};

	const FFailureCallback GenericFailure = [this](const FSequenceError Error)
	{
		UE_LOG(LogTemp, Display, TEXT("[Error: %s]"), *Error.Message);
		this->CallAuthFailure();
	};
	FDateTime cachedDate = FDateTime::UtcNow();
	this->AuthorizedRPC(this->BuildKMSAuthorizationHeader(cachedDate,URL.RightChop(8), RequestBody),cachedDate, URL, TEXT("TrentService.GenerateDataKey"), RequestBody, GenericSuccess, GenericFailure);
}

bool UAuthenticator::CanRetryEmailLogin()
{
	bool CanRetry = this->EmailAuthCurrRetries > 0;
	this->EmailAuthCurrRetries--;
	return CanRetry;
}

void UAuthenticator::ResetRetryEmailLogin()
{
	this->EmailAuthCurrRetries = this->EmailAuthMaxRetries;
}

void UAuthenticator::CognitoIdentityInitiateAuth(const FString& Email, const FString& AWSCognitoClientID)
{
	FString URL = BuildAWSURL(TEXT("cognito-idp"),this->Region);
	FString RequestBody = FString::Printf(TEXT("{\"AuthFlow\":\"CUSTOM_AUTH\",\"AuthParameters\":{\"USERNAME\":\"%s\"},\"ClientId\":\"%s\"}"), *Email, *AWSCognitoClientID);

	const TSuccessCallback<FString> GenericSuccess = [this](const FString response)
	{
		UE_LOG(LogTemp, Display, TEXT("Response %s"), *response);
		TSharedPtr<FJsonObject> responseObj = this->ResponseToJson(response);
		FString SessionPtr;
		if (responseObj->TryGetStringField(TEXT("Session"), SessionPtr))
		{//good state
			this->ChallengeSession = SessionPtr;
			this->CallAuthRequiresCode();
		}
		else
		{//error state
			if (response.Contains(TEXT("user not found"), ESearchCase::IgnoreCase))
			{//no user exists so create one!
				UE_LOG(LogTemp, Display, TEXT("Creating New User"));
				this->CognitoIdentitySignUp(this->Cached_Email, this->GenerateSignUpPassword(),this->CognitoClientID);
			}
			else
			{//unknown error
				UE_LOG(LogTemp, Error, TEXT("[Unexpected error when trying to authenticate]"));
				this->CallAuthFailure();
			}
		}
	};

	const FFailureCallback GenericFailure = [this](const FSequenceError Error)
	{
		UE_LOG(LogTemp, Display, TEXT("[Error: %s]"), *Error.Message);
		this->CallAuthFailure();
	};
	
	if (this->CanRetryEmailLogin())
	{
		this->RPC(URL, TEXT("AWSCognitoIdentityProviderService.InitiateAuth"), RequestBody, GenericSuccess, GenericFailure);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Max login attempts reached]"));
		this->CallAuthFailure();
	}
}

FString UAuthenticator::GenerateSignUpPassword()
{
	FString pw = TEXT("aB1%");
	const int32 len = 12;
	for (int i = 0; i < len; i++)
	{
		pw += this->PWCharList[FMath::RandRange(0,this->PWCharList.Num()-1)];
	}
	return pw;
}

void UAuthenticator::CognitoIdentitySignUp(const FString& Email, const FString& Password, const FString& AWSCognitoClientID)
{
	FString URL = BuildAWSURL(TEXT("cognito-idp"),this->Region);
	//FString RequestBody = TEXT("{\"ClientId\":\"") + FString::Printf(TEXT("%s"), *this->CognitoClientID) + TEXT("\",\"Password\":\"") + FString::Printf(TEXT("%s"), *Password) + TEXT("\",\"UserAttributes\":[{\"Name\":\"email\",\"Value\":\"") + FString::Printf(TEXT("%s"), *Email) + TEXT("\"}],\"Username\":\"") + FString::Printf(TEXT("%s"), *Email) + TEXT("\"}");
	FString RequestBody = FString::Printf(TEXT("{\"ClientId\":\"%s\",\"Password\":\"%s\",\"UserAttributes\":[{\"Name\":\"email\",\"Value\":\"%s\"}],\"Username\":\"%s\"}"), *AWSCognitoClientID, *Password, *Email, *Email);

	const TSuccessCallback<FString> GenericSuccess = [this](const FString response)
	{
		UE_LOG(LogTemp, Display, TEXT("Response %s"), *response);
		this->CognitoIdentityInitiateAuth(this->Cached_Email,this->CognitoClientID);
	};

	const FFailureCallback GenericFailure = [this](const FSequenceError Error)
	{
		UE_LOG(LogTemp, Display, TEXT("[Error: %s]"), *Error.Message);
		this->CallAuthFailure();
	};

	this->RPC(URL, TEXT("AWSCognitoIdentityProviderService.SignUp"), RequestBody, GenericSuccess, GenericFailure);
}

void UAuthenticator::AdminRespondToAuthChallenge(const FString& Email, const FString& Answer, const FString& ChallengeSessionString, const FString& AWSCognitoClientID)
{
	FString URL = BuildAWSURL(TEXT("cognito-idp"),this->Region);
	FString RequestBody = FString::Printf(TEXT("{\"ChallengeName\":\"CUSTOM_CHALLENGE\",\"ClientId\":\"%s\",\"Session\":\"%s\",\"ChallengeResponses\":{\"USERNAME\":\"%s\",\"ANSWER\":\"%s\"}}"), *AWSCognitoClientID, *ChallengeSessionString, *Email, *Answer);

	const TSuccessCallback<FString> GenericSuccess = [this](const FString response)
	{
		UE_LOG(LogTemp, Display, TEXT("Response %s"), *response);
		TSharedPtr<FJsonObject> responseObj = this->ResponseToJson(response);
		FString IDTokenPtr;
		const TSharedPtr<FJsonObject> *AuthObject;
		if (responseObj->TryGetObjectField(TEXT("AuthenticationResult"), AuthObject))
		{
			if (AuthObject->Get()->TryGetStringField(TEXT("IdToken"), IDTokenPtr))
			{//good state
				this->Cached_IDToken = IDTokenPtr;
				this->SetSocialLoginType(ESocialSigninType::AWS);
				this->SocialLogin(this->Cached_IDToken);
			}
			else
			{//error state
				UE_LOG(LogTemp, Error, TEXT("[No idToken found in AuthenticationResult]"));
				this->CallAuthFailure();
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[No Authentication Result found]"));
			this->CallAuthFailure();
		}
	};

	const FFailureCallback GenericFailure = [this](const FSequenceError Error)
	{
		UE_LOG(LogTemp, Display, TEXT("[Error: %s]"), *Error.Message);
		this->CallAuthFailure();
	};

	this->RPC(URL, TEXT("AWSCognitoIdentityProviderService.RespondToAuthChallenge"), RequestBody, GenericSuccess, GenericFailure);
}

TSharedPtr<FJsonObject> UAuthenticator::ResponseToJson(const FString& response)
{
	TSharedPtr<FJsonObject> responseObj;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(response), responseObj))
		return responseObj;
	else
		return nullptr;
}

void UAuthenticator::EmailLoginCode(const FString& CodeIn)
{
	this->AdminRespondToAuthChallenge(this->Cached_Email,CodeIn,this->ChallengeSession,this->CognitoClientID);
}

void UAuthenticator::AuthWithSequence(const FString& IDTokenIn, const TArray<uint8_t>& Key)
{
	//Generates a new Random Wallet
	this->SessionWallet = new FWallet();
	int64 UnixIssueTime = FDateTime::UtcNow().ToUnixTimestamp();
	int64 UnixExpireTime = UnixIssueTime + 30;
	FString UnixIssueString = FString::Printf(TEXT("%lld"), UnixIssueTime);
	FString UnixExpireString = FString::Printf(TEXT("%lld"), UnixExpireTime);

	FString CachedWalletAddress = FString::Printf(TEXT("0x%s"),*BytesToHex(this->SessionWallet->GetWalletAddress().Arr, this->SessionWallet->GetWalletAddress().GetLength()).ToLower());

	FString Intent = FString::Printf(TEXT("{\\\"version\\\":\\\"%s\\\",\\\"packet\\\":{\\\"code\\\":\\\"openSession\\\",\\\"expires\\\":%s,\\\"issued\\\":%s,\\\"session\\\":\\\"%s\\\",\\\"proof\\\":{\\\"idToken\\\":\\\"%s\\\"}}}"),*this->WaasVersion, *UnixExpireString, *UnixIssueString, *CachedWalletAddress, *IDTokenIn);
	FString Payload = FString::Printf(TEXT("{\"projectId\":%s,\"idToken\":\"%s\",\"sessionAddress\":\"%s\",\"friendlyName\":\"FRIENDLY SESSION WALLET\",\"intentJSON\":\"%s\"}"),*this->ProjectID, *IDTokenIn, *CachedWalletAddress, *Intent);
	UE_LOG(LogTemp, Display, TEXT("Payload: %s"), *Payload);

	TArray<uint8_t> TPayload = PKCS7(Payload);
	AES_ctx ctx;
	struct AES_ctx* PtrCtx = &ctx;

	const int32 IVSize = 16;
	TArray<uint8_t> iv;

	for (int i = 0; i < IVSize; i++)
		iv.Add(RandomByte());

	AES_init_ctx_iv(PtrCtx, Key.GetData(), iv.GetData());
	AES_CBC_encrypt_buffer(PtrCtx, TPayload.GetData(), TPayload.Num());

	FString PrePendedKey = BytesToHex(Key.GetData(),Key.Num());
	FString PrePendedIV = BytesToHex(iv.GetData(),iv.Num());
	FString PrePendedCipher = BytesToHex(TPayload.GetData(), TPayload.Num());

	FString PublicKey = BytesToHex(this->SessionWallet->GetWalletPublicKey().Arr,this->SessionWallet->GetWalletPublicKey().GetLength()).ToLower();
	FString PrivateKey = BytesToHex(this->SessionWallet->GetWalletPrivateKey().Arr,this->SessionWallet->GetWalletPrivateKey().GetLength()).ToLower();
	FString Address = BytesToHex(this->SessionWallet->GetWalletAddress().Arr,this->SessionWallet->GetWalletAddress().GetLength()).ToLower();

	FString PayloadCipherText = FString::Printf(TEXT("0x%s%s"), *BytesToHex(iv.GetData(), iv.Num()).ToLower(), *BytesToHex(TPayload.GetData(), TPayload.Num()).ToLower());
	
	TArray<uint8_t> PayloadSigBytes = this->SessionWallet->SignMessage(Payload);

	FString PayloadSig = FString::Printf(TEXT("0x%s"),*BytesToHex(PayloadSigBytes.GetData(), PayloadSigBytes.Num()).ToLower());
	FString EncryptedPayloadKey = FString::Printf(TEXT("0x%s"),*this->CipherTextBlob);
	FString FinalPayload = FString::Printf(TEXT("{\"payloadSig\":\"%s\",\"encryptedPayloadKey\":\"%s\",\"payloadCiphertext\":\"%s\"}"),*PayloadSig,*EncryptedPayloadKey,*PayloadCipherText);

	const TSuccessCallback<FString> GenericSuccess = [this](const FString response)
	{
		UE_LOG(LogTemp, Display, TEXT("Response %s"), *response);
		TSharedPtr<FJsonObject> responseObj = this->ResponseToJson(response);
		const TSharedPtr<FJsonObject>* SessionObj = nullptr;
		const TSharedPtr<FJsonObject>* DataObj = nullptr;
		FString PlainTextPtr, CipherTextBlobPtr;
		if (responseObj->TryGetObjectField(TEXT("session"), SessionObj) && responseObj->TryGetObjectField(TEXT("data"), DataObj))
		{//good state
			FString Id, Address, UserId, Subject, SessionId, Wallet, Issuer;
			if (SessionObj->Get()->TryGetStringField(TEXT("issuer"),Issuer) && SessionObj->Get()->TryGetStringField(TEXT("id"), Id) && SessionObj->Get()->TryGetStringField(TEXT("address"), Address) && SessionObj->Get()->TryGetStringField(TEXT("userId"), UserId) && SessionObj->Get()->TryGetStringField(TEXT("subject"), Subject) && DataObj->Get()->TryGetStringField(TEXT("sessionId"), SessionId) && DataObj->Get()->TryGetStringField(TEXT("wallet"), Wallet))
			{
				FString SessionPrivateKey = BytesToHex(this->SessionWallet->GetWalletPrivateKey().Arr, this->SessionWallet->GetWalletPrivateKey().GetLength()).ToLower();
				this->CallAuthSuccess(FCredentials_BE(this->PlainText, SessionPrivateKey, Id, Address, UserId, Subject, SessionId, Wallet, this->Cached_IDToken, this->Cached_Email, Issuer));
			}
			else
			{//error state
				UE_LOG(LogTemp, Error, TEXT("[Missing Credentials in Sequence Session Response]"));
				this->CallAuthFailure();
			}
		}
		else
		{//error state
			UE_LOG(LogTemp, Error, TEXT("[Missing Credentials in Sequence Response]"));
			this->CallAuthFailure();
		}
	};

	const FFailureCallback GenericFailure = [this](const FSequenceError Error)
	{
		UE_LOG(LogTemp, Display, TEXT("[Error: %s]"), *Error.Message);
		this->CallAuthFailure();
	};

	this->SequenceRPC(this->WaasAuthRPCURL, FinalPayload, GenericSuccess, GenericFailure);
}

void UAuthenticator::SequenceRPC(const FString& Url, const FString& RequestBody, TSuccessCallback<FString> OnSuccess, FFailureCallback OnFailure)
{
	NewObject<URequestHandler>()
		->PrepareRequest()
		->WithUrl(FString::Printf(TEXT("%s"), *Url))
		->WithHeader(TEXT("Content-type"), TEXT("application/json"))
		->WithHeader(TEXT("Accept"), TEXT("application/json"))
		->WithHeader(TEXT("X-Access-Key"), FString::Printf(TEXT("%s"), *this->ProjectAccessKey))
		->WithVerb(TEXT("POST"))
		->WithContentAsString(FString::Printf(TEXT("%s"), *RequestBody))
		->ProcessAndThen(OnSuccess, OnFailure);
}

void UAuthenticator::AuthorizedRPC(const FString& Authorization,const FDateTime& Date, const FString& Url, const FString& AMZTarget, const FString& RequestBody, TSuccessCallback<FString> OnSuccess, FFailureCallback OnFailure)
{
	FUnsizedData PayloadBytes = StringToUTF8(RequestBody);
	const Sha256Hash PayloadHashBytes = Sha256::getHash(PayloadBytes.Arr, PayloadBytes.GetLength());
	FString HashPayload = BytesToHex(PayloadHashBytes.value, PayloadHashBytes.HASH_LEN).ToLower();

	NewObject<URequestHandler>()
		->PrepareRequest()
		->WithUrl(FString::Printf(TEXT("%s"), *Url))
		->WithVerb(TEXT("POST"))
		->WithHeader(TEXT("Host"), FString::Printf(TEXT("%s"), *Url.RightChop(8)))
		->WithHeader(TEXT("Content-type"), TEXT("application/x-amz-json-1.1"))
		->WithHeader(TEXT("X-AMZ-TARGET"), FString::Printf(TEXT("%s"), *AMZTarget))
		->WithHeader(TEXT("X-AMZ-DATE"), FString::Printf(TEXT("%s"), *BuildFullDateTime(Date)))
		->WithHeader(TEXT("X-AMZ-CONTENT-SHA256"), FString::Printf(TEXT("%s"), *HashPayload))
		->WithHeader(TEXT("x-amz-security-token"), FString::Printf(TEXT("%s"), *this->SessionToken))
		->WithHeader(TEXT("Authorization"), FString::Printf(TEXT("%s"), *Authorization))
		->WithContentAsString(FString::Printf(TEXT("%s"), *RequestBody))
		->ProcessAndThen(OnSuccess, OnFailure);

	//cleanup
	delete[] PayloadBytes.Arr;
}

void UAuthenticator::RPC(const FString& Url, const FString& AMZTarget, const FString& RequestBody, TSuccessCallback<FString> OnSuccess, FFailureCallback OnFailure)
{
	NewObject<URequestHandler>()
		->PrepareRequest()
		->WithUrl(FString::Printf(TEXT("%s"), *Url))
		->WithHeader(TEXT("Content-type"), TEXT("application/x-amz-json-1.1"))
		->WithHeader(TEXT("X-AMZ-TARGET"), FString::Printf(TEXT("%s"), *AMZTarget))
		->WithVerb(TEXT("POST"))
		->WithContentAsString(FString::Printf(TEXT("%s"), *RequestBody))
		->ProcessAndThen(OnSuccess, OnFailure);
}