#pragma once
#include "Sequence/SequenceAPI.h"
#include "Eth/Crypto.h"
#include "Http.h"
#include "HttpManager.h"
#include "RequestHandler.h"
#include "Indexer/IndexerSupport.h"
#include "Util/JsonBuilder.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "AES/aes.h"
#include "Bitcoin-Cryptography-Library/cpp/Keccak256.hpp"
#include "Types/ContractCall.h"
#include "Util/HexUtility.h"

FString SequenceAPI::SortOrderToString(ESortOrder SortOrder)
{
	return UEnum::GetValueAsString(SortOrder);
}

ESortOrder SequenceAPI::StringToSortOrder(FString String)
{
	return String == "ASC" ? ESortOrder::ASC : ESortOrder::DESC;
}

FString SequenceAPI::FPage_Sequence::ToJson()
{
	FJsonBuilder Json = FJsonBuilder();

	if(PageSize.IsSet())
	{
		Json.AddInt("pageSize", PageSize.GetValue());
	}
	if(PageNum.IsSet())
	{
		Json.AddInt("page", PageNum.GetValue());
	}
	if(TotalRecords.IsSet())
	{
		Json.AddInt("totalRecords", TotalRecords.GetValue());
	}
	if(Column.IsSet())
	{
		Json.AddString("column", Column.GetValue());
	}
	if(Sort.IsSet())
	{
		FJsonArray Array = Json.AddArray("sort");

		for(FSortBy SortBy : Sort.GetValue())
		{
			Array.AddValue(SortBy.GetJsonString());
		}
	}
	
	return Json.ToString();
}

SequenceAPI::FPage_Sequence SequenceAPI::FPage_Sequence::Convert(FPage Page,int64 TotalRecords)
{
	SequenceAPI::FPage_Sequence Ret;
	Ret.Column = Page.column;
	Ret.PageNum = Page.page;
	Ret.PageSize = Page.pageSize;
	Ret.Sort = Page.sort;
	Ret.TotalRecords = TotalRecords;
	return Ret;
}

SequenceAPI::FPage_Sequence SequenceAPI::FPage_Sequence::From(TSharedPtr<FJsonObject> Json)
{
	FPage_Sequence page = FPage_Sequence{};

	if(Json->HasField("pageSize"))
	{
		page.PageSize = static_cast<uint64>(Json->GetIntegerField("pageSize")); 
	}
	if(Json->HasField("page"))
	{
		page.PageNum =  static_cast<uint64>(Json->GetIntegerField("page")); 
	}
	if(Json->HasField("totalRecords"))
	{
		page.TotalRecords = static_cast<uint64>(Json->GetIntegerField("totalRecords")); 
	}
	if(Json->HasField("Column"))
	{
		page.Column = Json->GetStringField("Column");
	}
	if(Json->HasField("Sort"))
	{
		TArray<TSharedPtr<FJsonValue>> JsonArray = Json->GetArrayField("Sort");
		TArray<FSortBy> Array;

		for(TSharedPtr<FJsonValue> JsonVal : JsonArray)
		{
			FSortBy Result;
			if (!FJsonObjectConverter::JsonObjectToUStruct<FSortBy>(JsonVal->AsObject().ToSharedRef(), &Result))
				Array.Push(Result);
		}
		
		page.Sort = Array;
	}

	return page;
}

//Hide for now
SequenceAPI::FTransaction_Sequence SequenceAPI::FTransaction_Sequence::Convert(FTransaction_FE Transaction_Fe)
{
	return FTransaction_Sequence{
		static_cast<uint64>(Transaction_Fe.chainId),
		FAddress::From(Transaction_Fe.From),
		FAddress::From(Transaction_Fe.To),
		Transaction_Fe.AutoGas == "" ? TOptional<FString>() : TOptional(Transaction_Fe.AutoGas),
		Transaction_Fe.Nonce < 0 ? TOptional<uint64>() : TOptional(static_cast<uint64>(Transaction_Fe.Nonce)),
		Transaction_Fe.Value == "" ? TOptional<FString>() : TOptional(Transaction_Fe.Value),
		Transaction_Fe.CallData == "" ? TOptional<FString>() : TOptional(Transaction_Fe.CallData),
		Transaction_Fe.TokenAddress == "" ? TOptional<FString>() : TOptional(Transaction_Fe.TokenAddress),
		Transaction_Fe.TokenAmount == "" ? TOptional<FString>() : TOptional(Transaction_Fe.TokenAmount),
		Transaction_Fe.TokenIds.Num() == 0 ? TOptional<TArray<FString>>() : TOptional(Transaction_Fe.TokenIds),
		Transaction_Fe.TokenAmounts.Num() == 0 ? TOptional<TArray<FString>>() : TOptional(Transaction_Fe.TokenAmounts),
	};
}

const FString SequenceAPI::FTransaction_Sequence::ToJson()
{
	FJsonBuilder Json = FJsonBuilder();

	Json.AddInt("chainId", ChainId);
	Json.AddString("from", "0x" + From.ToHex());
	Json.AddString("to", "0x" + To.ToHex());

	if(this->Value.IsSet()) Json.AddString("value", this->Value.GetValue());

	return Json.ToString();
}

const SequenceAPI::TransactionID SequenceAPI::FTransaction_Sequence::ID()
{
	FUnsizedData Data = StringToUTF8(ToJson());
	return GetKeccakHash(Data).ToHex();
}

SequenceAPI::FPartnerWallet SequenceAPI::FPartnerWallet::From(TSharedPtr<FJsonObject> Json)
{
	return FPartnerWallet{
		static_cast<uint64>(Json->GetIntegerField("number")),
		static_cast<uint64>(Json->GetIntegerField("partnerId")),
		static_cast<uint64>(Json->GetIntegerField("walletIndex")),
		Json->GetStringField("walletAddress")
	};
}

FString SequenceAPI::FSequenceWallet::Url(const FString Name) const
{
	return this->Hostname + this->Path + Name;
}

void SequenceAPI::FSequenceWallet::SendRPC(FString Url, FString Content, TSuccessCallback<FString> OnSuccess, FFailureCallback OnFailure)
{
	NewObject<URequestHandler>()
			->PrepareRequest()
			->WithUrl(Url)
			->WithHeader("Content-type", "application/json")
			->WithHeader("Authorization", this->AuthToken)
			->WithVerb("POST")
			->WithContentAsString(Content)
			->ProcessAndThen(OnSuccess, OnFailure);
}

//Pass in credentials in our constructor!
SequenceAPI::FSequenceWallet::FSequenceWallet()
{
	this->Indexer = NewObject<UIndexer>();
}

SequenceAPI::FSequenceWallet::FSequenceWallet(const FCredentials_BE& CredentialsIn)
{
	this->Credentials = CredentialsIn;
	this->Indexer = NewObject<UIndexer>();
	this->AuthToken = "Bearer " + this->Credentials.GetIDToken();
}

SequenceAPI::FSequenceWallet::FSequenceWallet(const FCredentials_BE& CredentialsIn, const FString& ProviderURL)
{
	this->Credentials = CredentialsIn;
	this->Indexer = NewObject<UIndexer>();
	this->AuthToken = "Bearer " + this->Credentials.GetIDToken();
	this->ProviderUrl = ProviderURL;
}

FString SequenceAPI::FSequenceWallet::GetWalletAddress()
{
	return this->Credentials.GetWalletAddress();
}

void SequenceAPI::FSequenceWallet::CreateWallet(TSuccessCallback<FAddress> OnSuccess,FFailureCallback OnFailure)
{
	TFunction<TResult<FAddress> (FString)> ExtractAddress = [=](FString Content)
	{
		TSharedPtr<FJsonObject> Json = Parse(Content);
		TResult<FAddress> Retval = MakeValue(FAddress{});
		if(!Json)
		{
			Retval = MakeError(FSequenceError{RequestFail, "Json did not parse"});
		}
		else
		{
			const FString AddressString = Json->GetStringField("address");
			Retval = MakeValue(FAddress::From(AddressString));
		}
		
		return Retval;
	};

	FString Content = FJsonBuilder().ToPtr()->ToString();
	
	this->SendRPCAndExtract(Url("CreateWallet"), Content, OnSuccess, ExtractAddress,OnFailure);
}

void SequenceAPI::FSequenceWallet::GetWalletAddress(TSuccessCallback<FAddress> OnSuccess, FFailureCallback OnFailure)
{
	TFunction<TResult<FAddress> (FString)> ExtractAddress = [=](FString Content)
	{
		TSharedPtr<FJsonObject> Json = Parse(Content);
		TResult<FAddress> Retval = MakeValue(FAddress{});

		if(!Json)
		{
			Retval = MakeError(FSequenceError{RequestFail, "Json did not parse"});
		}
		else
		{
			const FString AddressString = Json->GetStringField("address");
			Retval = MakeValue(FAddress::From(AddressString));
		}
		return Retval;
	};
	FString Content = FJsonBuilder().ToPtr()->ToString();
	//this->SequenceRPC(Url("GetWalletAddress"), Content, OnSuccess, ExtractAddress,OnFailure);
}

void SequenceAPI::FSequenceWallet::DeployWallet(uint64 ChainId,TSuccessCallback<FDeployWalletReturn> OnSuccess, FFailureCallback OnFailure)
{
	TFunction<TResult<FDeployWalletReturn> (FString)> ExtractDeployWalletReturn = [=](FString Content)
	{
		
		TSharedPtr<FJsonObject> Json = Parse(Content);
		TResult<FDeployWalletReturn> Retval = MakeValue(FDeployWalletReturn{"", ""});

		if(!Json)
		{
			Retval = MakeError(FSequenceError{RequestFail, "Json did not parse"});
		}
		else
		{
			const FString AddressString = Json->GetStringField("address");
			const FString TransactionHashString = Json->GetStringField("txnHash");
			Retval = MakeValue(FDeployWalletReturn{AddressString, TransactionHashString});
		}		
		return Retval;
	};

	FString Content = FJsonBuilder().ToPtr()
		->AddInt("chainId", ChainId)
		->ToString();
	
	this->SendRPCAndExtract(Url("DeployWallet"), Content, OnSuccess, ExtractDeployWalletReturn,
	OnFailure);
}

void SequenceAPI::FSequenceWallet::Wallets(FPage_Sequence Page, TSuccessCallback<FWalletsReturn> OnSuccess,
	FFailureCallback OnFailure)
{
	const TFunction<TResult<FWalletsReturn> (FString)> ExtractWallets = [=](FString Content)
	{
		
		TSharedPtr<FJsonObject> Json = Parse(Content);
		TResult<FWalletsReturn> ReturnVal = MakeValue(FWalletsReturn{});

		if(!Json)
		{
			ReturnVal = MakeError(FSequenceError{RequestFail, "Json did not parse"});
		}
		else
		{
			TArray<TSharedPtr<FJsonValue>> WalletsJsonArray = Json->GetArrayField("wallets");
			const TSharedPtr<FJsonObject> PageJson = Json->GetObjectField("page");
			TArray<FPartnerWallet> Wallets;

			for(TSharedPtr<FJsonValue> WalletJson : WalletsJsonArray)
			{
				Wallets.Push(FPartnerWallet::From(WalletJson->AsObject()));
			}
			
			ReturnVal = MakeValue(FWalletsReturn{Wallets, FPage_Sequence::From(PageJson)});
		}
		
		return ReturnVal;
	};

	const FString Content = FJsonBuilder().ToPtr()
          ->AddField("page", Page.ToJson())
          ->ToString();
	
	this->SendRPCAndExtract(Url("Wallets"), Content, OnSuccess, ExtractWallets,
	OnFailure); 
}

void SequenceAPI::FSequenceWallet::Wallets(TSuccessCallback<FWalletsReturn> OnSuccess, FFailureCallback OnFailure)
{
	this->Wallets(FPage_Sequence{}, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::IsValidMessageSignature(uint64 ChainId, FAddress WalletAddress, FUnsizedData Message,
	FSignature Signature, TSuccessCallback<bool> OnSuccess, FFailureCallback OnFailure)
{
	const TFunction<TResult<bool> (FString)> ExtractSignature = [=](FString Content)
	{
		
		TSharedPtr<FJsonObject> Json = Parse(Content);
		TResult<bool> ReturnVal = MakeValue(false);

		if(!Json)
		{
			ReturnVal = MakeError(FSequenceError{RequestFail, "Json did not parse"});
		}
		else
		{
			const bool bIsValid = Json->GetBoolField("isValid");
			ReturnVal = MakeValue(bIsValid);
		}
		
		return ReturnVal;
	};

	const FString Content = FJsonBuilder().ToPtr()
		  ->AddInt("chainId", ChainId)
		  ->AddString("walletAddress", "0x" + WalletAddress.ToHex())
		  ->AddString("message", "0x" + Message.ToHex())
		  ->AddString("signature", "0x" + Signature.ToHex())
		  ->ToString();
	
	this->SendRPCAndExtract(Url("isValidMessageSignature"), Content, OnSuccess, ExtractSignature,
	OnFailure);
}

void SequenceAPI::FSequenceWallet::SendTransaction(FTransaction_Sequence Transaction, TSuccessCallback<FHash256> OnSuccess,
	FFailureCallback OnFailure)
{
	const TFunction<TResult<FHash256> (FString)> ExtractSignature = [=](FString Content)
	{
		
		TSharedPtr<FJsonObject> Json = Parse(Content);
		TResult<FHash256> ReturnVal = MakeValue(FHash256{});

		if(!Json)
		{
			ReturnVal = MakeError(FSequenceError{RequestFail, "Json did not parse"});
		}
		else
		{
			const FString HashString = Json->GetStringField("txHash");
			ReturnVal = MakeValue(FHash256::From(HashString));
		}
		
		return ReturnVal;
	};

	const FString Content = FJsonBuilder().ToPtr()
		  ->AddField("tx", Transaction.ToJson())
		  ->ToString();
	
	this->SendRPCAndExtract(Url("SendTransaction"), Content, OnSuccess, ExtractSignature,OnFailure);
}

void SequenceAPI::FSequenceWallet::RegisterSession(const TSuccessCallback<FString>& OnSuccess, const FFailureCallback& OnFailure)
{
	this->SequenceRPC("https://dev-waas.sequence.app/rpc/WaasAuthenticator/RegisterSession",this->GenerateSignedEncryptedRegisterSessionPayload(this->BuildRegisterSessionIntent()),OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::ListSessions(const TSuccessCallback<FString>& OnSuccess, const FFailureCallback& OnFailure)
{
	this->SequenceRPC("https://dev-waas.sequence.app/rpc/WaasAuthenticator/ListSessions",this->GenerateSignedEncryptedPayload(this->BuildListSessionIntent()),OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::CloseSession(const TSuccessCallback<FString>& OnSuccess, const FFailureCallback& OnFailure)
{
	this->SequenceRPC("https://dev-waas.sequence.app/rpc/WaasAuthenticator/SendIntent",this->GenerateSignedEncryptedPayload(this->BuildCloseSessionIntent()),OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::SessionValidation(const TSuccessCallback<FString>& OnSuccess, const FFailureCallback& OnFailure)
{
	OnSuccess("[RPC_NotActive]");
}

FString SequenceAPI::FSequenceWallet::GeneratePacketSignature(const FString& Packet) const
{
	//keccakhash of the packet first
	const FHash256 SigningHash = FHash256::New();
	const FUnsizedData EncodedSigningData = StringToUTF8(Packet);
	Keccak256::getHash(EncodedSigningData.Arr, EncodedSigningData.GetLength(), SigningHash.Arr);
	const FString Signature = "0x" + this->Credentials.SignMessageWithSessionWallet(BytesToHex(SigningHash.Arr,SigningHash.GetLength()));
	return Signature;
}

FString SequenceAPI::FSequenceWallet::GenerateSignedEncryptedRegisterSessionPayload(const FString& Intent) const
{
	const FString PreEncryptedPayload = "{\"projectId\":"+this->Credentials.GetProjectId()+",\"idToken\":\""+this->Credentials.GetIDToken()+"\",\"sessionAddress\":\""+this->Credentials.GetSessionWalletAddress()+"\",\"friendlyName\":\"FRIENDLY SESSION WALLET\",\"intentJSON\":\""+Intent+"\"}";
	UE_LOG(LogTemp,Display,TEXT("PreEncryptedPayload:\n%s"),*PreEncryptedPayload);
	return SignAndEncryptPayload(PreEncryptedPayload,Intent);
}

FString SequenceAPI::FSequenceWallet::GenerateSignedEncryptedPayload(const FString& Intent) const
{
	const FString PreEncryptedPayload = "{\"sessionId\":\""+this->Credentials.GetSessionId()+"\",\"intentJson\":\""+Intent+"\"}";
	UE_LOG(LogTemp,Display,TEXT("PreEncryptedPayload:\n%s"),*PreEncryptedPayload);
	return SignAndEncryptPayload(PreEncryptedPayload,Intent);
}

FString SequenceAPI::FSequenceWallet::SignAndEncryptPayload(const FString& PrePayload, const FString& Intent) const
{
	TArray<uint8_t> TPayload = PKCS7(PrePayload);
	AES_ctx ctx;
	struct AES_ctx* PtrCtx = &ctx;
	constexpr int32 IVSize = 16;
	TArray<uint8_t> iv;

	for (int i = 0; i < IVSize; i++)
		iv.Add(RandomByte());

	AES_init_ctx_iv(PtrCtx,this->Credentials.GetTransportKey().GetData(), iv.GetData());
	AES_CBC_encrypt_buffer(PtrCtx, TPayload.GetData(), TPayload.Num());
	const FString PayloadCipherText = "0x" + BytesToHex(iv.GetData(), iv.Num()).ToLower() + BytesToHex(TPayload.GetData(), TPayload.Num()).ToLower();
	const FString PayloadSig = "0x" + this->Credentials.SignMessageWithSessionWallet(Intent);
	const FString EncryptedPayloadKey = "0x" + this->Credentials.GetEncryptedPayloadKey();
	FString FinalPayload = "{\"encryptedPayloadKey\":\""+ EncryptedPayloadKey +"\",\"payloadCiphertext\":\""+ PayloadCipherText +"\",\"payloadSig\":\""+PayloadSig+"\"}";
	UE_LOG(LogTemp,Display,TEXT("FinalPayload: %s"),*FinalPayload);
	return FinalPayload;
}

void SequenceAPI::FSequenceWallet::SignMessage(const FString& Message, const TSuccessCallback<FString>& OnSuccess, const FFailureCallback& OnFailure)
{
	this->SequenceRPC("https://dev-waas.sequence.app/rpc/WaasAuthenticator/SendIntent",this->GenerateSignedEncryptedPayload(this->BuildSignMessageIntent(Message)),OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::SendSequenceTransaction(FSequenceTransaction,TSuccessCallback<FString> OnSuccess, FFailureCallback OnFailure)
{
}

void SequenceAPI::FSequenceWallet::SendERC20Transaction(FERC20Transaction,TSuccessCallback<FString> OnSuccess, FFailureCallback OnFailure)
{
	
}

void SequenceAPI::FSequenceWallet::SendERC721Transaction(FERC721Transaction,TSuccessCallback<FString> OnSuccess, FFailureCallback OnFailure)
{
	
}

void SequenceAPI::FSequenceWallet::SendERC1155Transaction(FERC1155Transaction,TSuccessCallback<FString> OnSuccess, FFailureCallback OnFailure)
{
	
}

FString SequenceAPI::FSequenceWallet::BuildSignMessageIntent(const FString& message)
{
	const int64 issued = FDateTime::UtcNow().ToUnixTimestamp();
	const int64 expires = issued + 86400;
	const FString issuedString = FString::Printf(TEXT("%lld"),issued);
	const FString expiresString = FString::Printf(TEXT("%lld"),expires);
	const FString Packet = "{\\\"code\\\":\\\"signMessage\\\",\\\"expires\\\":"+expiresString+",\\\"issued\\\":"+issuedString+",\\\"message\\\":\\\""+message+"\\\",\\\"network\\\":\\\""+this->Credentials.GetNetworkString()+"\\\",\\\"wallet\\\":\\\""+this->Credentials.GetWalletAddress()+"\\\"}";
	const FString Signature = this->GeneratePacketSignature(Packet);
	FString Intent = "{\\\"version\\\":\\\""+this->Credentials.GetWaasVersin()+"\\\",\\\"packet\\\":"+Packet+",\\\"signatures\\\":[{\\\"session\\\":\\\""+this->Credentials.GetSessionId()+"\\\",\\\"signature\\\":\\\""+Signature+"\\\"}]}";
	UE_LOG(LogTemp,Display,TEXT("SignMessageIntent: %s"),*Intent);
	return Intent;
}

FString SequenceAPI::FSequenceWallet::BuildSendTransactionIntent(const FString& Identifier, const FString& Txns)
{
	const int64 issued = FDateTime::UtcNow().ToUnixTimestamp();
	const int64 expires = issued + 86400;
	const FString issuedString = FString::Printf(TEXT("%lld"),issued);
	const FString expiresString = FString::Printf(TEXT("%lld"),expires);
	const FString Packet = "{\"code\":\"sendTransaction\",\"expires\":"+expiresString+",\"identifier\":\""+Identifier+"\",\"issued\":"+issuedString+",\"network\":\""+this->Credentials.GetNetworkString()+"\",\"transactions\":"+Txns+",\"wallet\":\""+this->Credentials.GetWalletAddress()+"\"}";
	const FString Signature = this->GeneratePacketSignature(Packet);
	FString Intent = "{\"version\":\""+this->Credentials.GetWaasVersin()+"\",\"packet\":"+Packet+",\"signatures\":[{\"session\":\""+this->Credentials.GetSessionId()+"\",\"signature\":\""+Signature+"\"}]}";
	UE_LOG(LogTemp,Display,TEXT("SendTransactionIntent: %s"),*Intent);
	return Intent;
}

/*
 * {
  "projectId": 2,
  "idToken": "eyJhbGciOiJSUzI1NiIsImtpZCI6Ijg1ZTU1MTA3NDY2YjdlMjk4MzYxOTljNThjNzU4MWY1YjkyM2JlNDQiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJhY2NvdW50cy5nb29nbGUuY29tIiwiYXpwIjoiOTcwOTg3NzU2NjYwLTM1YTZ0YzQ4aHZpOGNldjljbmtucDBpdWd2OXBvYTIzLmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tIiwiYXVkIjoiOTcwOTg3NzU2NjYwLTM1YTZ0YzQ4aHZpOGNldjljbmtucDBpdWd2OXBvYTIzLmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tIiwic3ViIjoiMTEzMzIzMTA1NzkwNDExNTI4MjEyIiwiaGQiOiJob3Jpem9uLmlvIiwiZW1haWwiOiJxcEBob3Jpem9uLmlvIiwiZW1haWxfdmVyaWZpZWQiOnRydWUsImNfaGFzaCI6IjJqV09fUWxLM1BZSDdkQ1hRQUNaSXciLCJub25jZSI6ImMwZTIwZTlhLTNlNjEtNGRkZS05YTc4LTIzYmQ3YjM5YzhiMiIsIm5iZiI6MTcwNjEwOTIzMywiaWF0IjoxNzA2MTA5NTMzLCJleHAiOjE3MDYxMTMxMzMsImp0aSI6ImQ4NGUwMmU3MjkzZmM5NjVmZGE1ZmZkYWY3Nzk2MDkyOGQ4NGIxOWYifQ.EA03UZhFxjyOwWmhAA94CT9A5VseebA0SfaDRwGTl9E6qYTiXjVfDaPhseGJ10i8lW7bIhH0L-hzLEzEZHESaaccIw1BJ-zCe7-qVn_b_0f4nu1xWTvxJoaoWObhQ4MCVtArhO0C-_Du5HFBrVh0MjAfLHnbGVyqu2vllTiZ0-aXMpg1mRL4vTFATPNAJeRD-MTeGqXSu5kOVzzp8-8ondlW7NVBYc4E87YLNsLdwSYY2uKkgw2CPCQfiDTkylnaCWmsfvm3PQMKDHJjrULqo1VDuLTZf0jO7U9myYKVvV_dAGBUC1XWjgNXnzBH6USg2SE6lHtQM7DVUxG1EVr-bQ",
  "sessionAddress": "0x4d79F518257eD887E73787e93653C5977587e6Aa",
  "friendlyName": "FRIENDLY SESSION WALLET",
  "intentJSON": "{\"version\":\"1.0.0\",\"packet\":{\"code\":\"openSession\",\"expires\":1706109565,\"issued\":1706109535,\"session\":\"0x4d79F518257eD887E73787e93653C5977587e6Aa\",\"proof\":{\"idToken\":\"eyJhbGciOiJSUzI1NiIsImtpZCI6Ijg1ZTU1MTA3NDY2YjdlMjk4MzYxOTljNThjNzU4MWY1YjkyM2JlNDQiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJhY2NvdW50cy5nb29nbGUuY29tIiwiYXpwIjoiOTcwOTg3NzU2NjYwLTM1YTZ0YzQ4aHZpOGNldjljbmtucDBpdWd2OXBvYTIzLmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tIiwiYXVkIjoiOTcwOTg3NzU2NjYwLTM1YTZ0YzQ4aHZpOGNldjljbmtucDBpdWd2OXBvYTIzLmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tIiwic3ViIjoiMTEzMzIzMTA1NzkwNDExNTI4MjEyIiwiaGQiOiJob3Jpem9uLmlvIiwiZW1haWwiOiJxcEBob3Jpem9uLmlvIiwiZW1haWxfdmVyaWZpZWQiOnRydWUsImNfaGFzaCI6IjJqV09fUWxLM1BZSDdkQ1hRQUNaSXciLCJub25jZSI6ImMwZTIwZTlhLTNlNjEtNGRkZS05YTc4LTIzYmQ3YjM5YzhiMiIsIm5iZiI6MTcwNjEwOTIzMywiaWF0IjoxNzA2MTA5NTMzLCJleHAiOjE3MDYxMTMxMzMsImp0aSI6ImQ4NGUwMmU3MjkzZmM5NjVmZGE1ZmZkYWY3Nzk2MDkyOGQ4NGIxOWYifQ.EA03UZhFxjyOwWmhAA94CT9A5VseebA0SfaDRwGTl9E6qYTiXjVfDaPhseGJ10i8lW7bIhH0L-hzLEzEZHESaaccIw1BJ-zCe7-qVn_b_0f4nu1xWTvxJoaoWObhQ4MCVtArhO0C-_Du5HFBrVh0MjAfLHnbGVyqu2vllTiZ0-aXMpg1mRL4vTFATPNAJeRD-MTeGqXSu5kOVzzp8-8ondlW7NVBYc4E87YLNsLdwSYY2uKkgw2CPCQfiDTkylnaCWmsfvm3PQMKDHJjrULqo1VDuLTZf0jO7U9myYKVvV_dAGBUC1XWjgNXnzBH6USg2SE6lHtQM7DVUxG1EVr-bQ\"}}}"
}

* "{\"version\":\"1.0.0\",\"packet\":{\"code\":\"openSession\",\"expires\":1706109565,\"issued\":1706109535,\"session\":\"sessionAddress\",\"proof\":{\"idToken\":\"idToken\"}}}"
*/

FString SequenceAPI::FSequenceWallet::BuildRegisterSessionIntent()
{
	const int64 issued = FDateTime::UtcNow().ToUnixTimestamp();
	const int64 expires = issued + 86400;
	const FString issuedString = FString::Printf(TEXT("%lld"),issued);
	const FString expiresString = FString::Printf(TEXT("%lld"),expires);
	const FString Packet = "{\\\"code\\\":\\\"openSession\\\",\\\"expires\\\":"+expiresString+",\\\"issued\\\":"+issuedString+",\\\"session\\\":\\\""+this->Credentials.GetSessionId()+"\\\",\\\"proof\\\":{\\\"idToken\\\":\\\""+this->Credentials.GetIDToken()+"\\\"}}";
	const FString Intent = "{\\\"version\\\":\\\""+this->Credentials.GetWaasVersin()+"\\\",\\\"packet\\\":"+Packet+"}";
	UE_LOG(LogTemp,Display,TEXT("RegisterSessionIntent: %s"),*Intent);
	return Intent;
}

FString SequenceAPI::FSequenceWallet::BuildListSessionIntent()
{
	const FString Intent = "{\\\"sessionId\\\":\\\""+this->Credentials.GetSessionId()+"\\\"}";
	UE_LOG(LogTemp,Display,TEXT("ListSessionIntent: %s"),*Intent);
	return Intent;
}

FString SequenceAPI::FSequenceWallet::BuildCloseSessionIntent()
{
	const int64 issued = FDateTime::UtcNow().ToUnixTimestamp();
	const int64 expires = issued + 86400;
	const FString issuedString = FString::Printf(TEXT("%lld"),issued);
	const FString expiresString = FString::Printf(TEXT("%lld"),expires);
	const FString Packet = "{\\\"code\\\":\\\"closeSession\\\",\\\"expires\\\":"+expiresString+",\\\"issued\\\":"+issuedString+",\\\"session\\\":\\\""+this->Credentials.GetSessionId()+"\\\",\\\"wallet\\\":\\\""+this->GetWalletAddress()+"\\\"}";
	const FString Signature = this->GeneratePacketSignature(Packet);
	FString Intent = "{\\\"version\\\":\\\""+this->Credentials.GetWaasVersin()+"\\\",\\\"packet\\\":"+Packet+",\\\"signatures\\\":[{\\\"session\\\":\\\""+this->Credentials.GetSessionId()+"\\\",\\\"signatures\\\":\\\""+Signature+"\\\"}]}";
	UE_LOG(LogTemp,Display,TEXT("CloseSessionIntent: %s"),*Intent);
	return Intent;
}

FString SequenceAPI::FSequenceWallet::BuildSessionValidationIntent()
{
	const FString Intent = "{\\\"sessionId\\\":\\\""+this->Credentials.GetSessionId()+"\\\"}";
	UE_LOG(LogTemp,Display,TEXT("SessionValidationIntent: %s"),*Intent);
	return Intent;
}

void SequenceAPI::FSequenceWallet::SendTransactionBatch(TArray<FTransaction_Sequence> Transactions,
	TSuccessCallback<FHash256> OnSuccess, FFailureCallback OnFailure)
{
	const TFunction<TResult<FHash256> (FString)> ExtractSignature = [=](FString Content)
	{
		
		TResult<TSharedPtr<FJsonObject>> Json = ExtractJsonObjectResult(Content);
		TResult<FHash256> ReturnVal = MakeValue(FHash256{});

		if(Json.HasError())
		{
			ReturnVal = MakeError(Json.GetError());
		}
		else
		{
			const FString HashString = Json.GetValue()->GetStringField("txHash");
			ReturnVal = MakeValue(FHash256::From(HashString));
		}
		
		return ReturnVal;
	};

	FJsonArray JsonArray = FJsonBuilder().AddArray("txs");
	for(FTransaction_Sequence Transaction : Transactions)
	{
		JsonArray.AddValue(Transaction.ToJson());
	}
	const FString Content = JsonArray.EndArray()->ToString();
	
	this->SendRPCAndExtract(Url("sendTransactionBatch"), Content, OnSuccess, ExtractSignature,
	OnFailure);
}

void SequenceAPI::FSequenceWallet::SendTransactionWithCallback(FTransaction_FE Transaction,
                                                               TSuccessCallback<TransactionID> OnSuccess, TFunction<void(TransactionID, FSequenceError)> OnFailure)
{
	FTransaction_Sequence Converted = FTransaction_Sequence::Convert(Transaction);
	FString ID = Converted.ID();
	SendTransaction(Converted, [=](FHash256 Hash)
	{
		OnSuccess(ID);
	}, [=](FSequenceError Error)
	{
		OnFailure(ID, Error);
	});
}

template <typename T> void SequenceAPI::FSequenceWallet::SequenceRPC(FString Url, FString Content, TSuccessCallback<T> OnSuccess, FFailureCallback OnFailure)
{
	NewObject<URequestHandler>()
	->PrepareRequest()
	->WithUrl(Url)
	->WithHeader("Content-type", "application/json")
	->WithHeader("Accept", "application/json")
	->WithHeader("X-Access-Key", Credentials.GetProjectAccessKey())
	->WithVerb("POST")
	->WithContentAsString(Content)
	->ProcessAndThen(OnSuccess, OnFailure);
}

FString SequenceAPI::FSequenceWallet::getSequenceURL(FString endpoint)
{
	return this->sequenceURL + endpoint;
}

TArray<FContact_BE> SequenceAPI::FSequenceWallet::buildFriendListFromJson(FString json)
{
	TArray<FContact_BE> friendList;
	TSharedPtr<FJsonObject> jsonObj;

	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(json), jsonObj))
	{
		const TArray<TSharedPtr<FJsonValue>>* storedFriends;
		if (jsonObj.Get()->TryGetArrayField("friends", storedFriends))
		{
			for (TSharedPtr<FJsonValue> friendData : *storedFriends)
			{
				const TSharedPtr<FJsonObject>* fJsonObj;
				if (friendData.Get()->TryGetObject(fJsonObj))//need it as an object
				{
					FContact_BE newFriend;
					newFriend.Public_Address = fJsonObj->Get()->GetStringField("userAddress");
					newFriend.Nickname = fJsonObj->Get()->GetStringField("nickname");
					friendList.Add(newFriend);
				}
			}
		}
	}
	else
	{//failure
		UE_LOG(LogTemp, Error, TEXT("Failed to convert String: %s to Json object"), *json);
	}
	return friendList;
}

//DEPRECATED
/*
* Gets the friend data from the given username!
* This function appears to require some form of authentication (perhaps all of the sequence api does)
* @Deprecated
*/
void SequenceAPI::FSequenceWallet::getFriends(FString username, TSuccessCallback<TArray<FContact_BE>> OnSuccess, FFailureCallback OnFailure)
{
	FString json_arg = "{}";
	
	SendRPC(getSequenceURL("friendList"), json_arg, [=](FString Content)
		{
			OnSuccess(buildFriendListFromJson(Content));
		}, OnFailure);
}

TArray<FItemPrice_BE> SequenceAPI::FSequenceWallet::buildItemUpdateListFromJson(FString json)
{
	UE_LOG(LogTemp, Error, TEXT("Received json: %s"), *json);
	TSharedPtr<FJsonObject> jsonObj;
	FUpdatedPriceReturn updatedPrices;

	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(json), jsonObj))
	{
		if (FJsonObjectConverter::JsonObjectToUStruct<FUpdatedPriceReturn>(jsonObj.ToSharedRef(), &updatedPrices))
		{
			return updatedPrices.tokenPrices;
		}
	}
	else
	{//failure
		UE_LOG(LogTemp, Error, TEXT("Failed to convert String: %s to Json object"), *json);
	}
	TArray<FItemPrice_BE> updatedItems;
	return updatedItems;
}

void SequenceAPI::FSequenceWallet::getUpdatedCoinPrice(FID_BE itemToUpdate, TSuccessCallback<TArray<FItemPrice_BE>> OnSuccess, FFailureCallback OnFailure)
{
	TArray<FID_BE> items;
	items.Add(itemToUpdate);
	getUpdatedCoinPrices(items, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::getUpdatedCoinPrices(TArray<FID_BE> itemsToUpdate, TSuccessCallback<TArray<FItemPrice_BE>> OnSuccess, FFailureCallback OnFailure)
{
	FString args = "{\"tokens\":";
	FString jsonObjString = "";
	TArray<FString> parsedItems;
	for (FID_BE item : itemsToUpdate)
	{
		if (FJsonObjectConverter::UStructToJsonObjectString<FID_BE>(item, jsonObjString))
			parsedItems.Add(jsonObjString);
	}
	args += UIndexerSupport::stringListToSimpleString(parsedItems);
	args += "}";

	SendRPC(getSequenceURL("getCoinPrices"), args, [=](FString Content)
		{
			OnSuccess(buildItemUpdateListFromJson(Content));
		}, OnFailure);
}

void SequenceAPI::FSequenceWallet::getUpdatedCollectiblePrice(FID_BE itemToUpdate, TSuccessCallback<TArray<FItemPrice_BE>> OnSuccess, FFailureCallback OnFailure)
{
	TArray<FID_BE> items;
	items.Add(itemToUpdate);
	getUpdatedCollectiblePrices(items, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::getUpdatedCollectiblePrices(TArray<FID_BE> itemsToUpdate, TSuccessCallback<TArray<FItemPrice_BE>> OnSuccess, FFailureCallback OnFailure)
{
	FString args = "{\"tokens\":";
	FString jsonObjString = "";
	TArray<FString> parsedItems;
	for (FID_BE item : itemsToUpdate)
	{
		if (FJsonObjectConverter::UStructToJsonObjectString<FID_BE>(item, jsonObjString))
			parsedItems.Add(jsonObjString);
	}
	args += UIndexerSupport::stringListToSimpleString(parsedItems);
	args += "}";

	SendRPC(getSequenceURL("getCollectiblePrices"), args, [=](FString Content)
		{
			OnSuccess(buildItemUpdateListFromJson(Content));
		}, OnFailure);
}

FString SequenceAPI::FSequenceWallet::buildQR_Request_URL(FString walletAddress,int32 size)
{
	int32 lclSize = FMath::Max(size, 64);//ensures a nice valid size

	FString urlSize = "/";
	urlSize.AppendInt(size);

	return sequenceURL_QR + encodeB64_URL(walletAddress) + urlSize;
}

//we only need to encode base64URL we don't decode them as we receive the QR code
FString SequenceAPI::FSequenceWallet::encodeB64_URL(FString data)
{
	FString ret;
	UE_LOG(LogTemp, Display, TEXT("Pre encoded addr: [%s]"), *data);
	ret = FBase64::Encode(data);
	UE_LOG(LogTemp, Display, TEXT("Post encoded addr: [%s]"), *ret);
	//now we just gotta do some swaps to make it base64 URL complient
	// + -> -
	// / -> _ 

	FString srch_plus = TEXT("+");
	FString rep_plus = TEXT("-");
	FString srch_slash = TEXT("/");
	FString rep_slash = TEXT("_");

	const TCHAR* srch_ptr_plus = *srch_plus;
	const TCHAR* rep_ptr_plus = *rep_plus;
	const TCHAR* srch_ptr_slash = *srch_slash;
	const TCHAR* rep_ptr_slash = *rep_slash;

	ret.ReplaceInline(srch_ptr_plus, rep_ptr_plus, ESearchCase::IgnoreCase);//remove + and replace with -
	ret.ReplaceInline(srch_ptr_slash, rep_ptr_slash, ESearchCase::IgnoreCase);//remove / and replace with _

	UE_LOG(LogTemp, Display, TEXT("B64-URL encoded addr: [%s]"), *ret);

	return ret;
}

//Indexer Calls

void SequenceAPI::FSequenceWallet::Ping(int64 chainID, TSuccessCallback<bool> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->Ping(chainID, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::Version(int64 chainID, TSuccessCallback<FVersion> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->Version(chainID,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::RunTimeStatus(int64 chainID, TSuccessCallback<FRuntimeStatus> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->RunTimeStatus(chainID, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::GetChainID(int64 chainID, TSuccessCallback<int64> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->GetChainID(chainID, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::GetEtherBalance(int64 chainID, FString accountAddr, TSuccessCallback<FEtherBalance> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->GetEtherBalance(chainID, accountAddr, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::GetTokenBalances(int64 chainID, FGetTokenBalancesArgs args, TSuccessCallback<FGetTokenBalancesReturn> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->GetTokenBalances(chainID, args, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::GetTokenSupplies(int64 chainID, FGetTokenSuppliesArgs args, TSuccessCallback<FGetTokenSuppliesReturn> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->GetTokenSupplies(chainID, args, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::GetTokenSuppliesMap(int64 chainID, FGetTokenSuppliesMapArgs args, TSuccessCallback<FGetTokenSuppliesMapReturn> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->GetTokenSuppliesMap(chainID, args, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::GetBalanceUpdates(int64 chainID, FGetBalanceUpdatesArgs args, TSuccessCallback<FGetBalanceUpdatesReturn> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->GetBalanceUpdates(chainID, args, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::GetTransactionHistory(int64 chainID, FGetTransactionHistoryArgs args, TSuccessCallback<FGetTransactionHistoryReturn> OnSuccess, FFailureCallback OnFailure)
{
	if (this->Indexer)
		this->Indexer->GetTransactionHistory(chainID, args, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::BlockByNumber(uint64 Number, TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).BlockByNumber(Number, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::BlockByNumber(EBlockTag Tag, TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).BlockByNumber(Tag,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::BlockByHash(FHash256 Hash, TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).BlockByHash(Hash, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::BlockNumber(TSuccessCallback<uint64> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).BlockNumber(OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::HeaderByNumber(uint64 Id, TSuccessCallback<FHeader> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).HeaderByNumber(Id, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::HeaderByNumber(EBlockTag Tag, TSuccessCallback<FHeader> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).HeaderByNumber(Tag, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::HeaderByHash(FHash256 Hash, TSuccessCallback<FHeader> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).HeaderByHash(Hash,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::TransactionByHash(FHash256 Hash, TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).TransactionByHash(Hash,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::TransactionCount(FAddress Addr, uint64 Number, TSuccessCallback<uint64> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).TransactionCount(Addr,Number,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::TransactionCount(FAddress Addr, EBlockTag Tag, TSuccessCallback<uint64> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).TransactionCount(Addr,Tag,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::TransactionReceipt(FHash256 Hash, TSuccessCallback<FTransactionReceipt> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).TransactionReceipt(Hash,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::GetGasPrice(TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).GetGasPrice(OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::EstimateContractCallGas(FContractCall ContractCall, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).EstimateContractCallGas(ContractCall,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::EstimateDeploymentGas(FAddress From, FString Bytecode, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).EstimateDeploymentGas(From,Bytecode,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::DeployContract(FString Bytecode, FPrivateKey PrivKey, int64 ChainId, TSuccessCallback<FAddress> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).DeployContract(Bytecode, PrivKey, ChainId, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::DeployContractWithHash(FString Bytecode, FPrivateKey PrivKey, int64 ChainId, TSuccessCallbackTuple<FAddress, FUnsizedData> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).DeployContractWithHash(Bytecode,PrivKey,ChainId,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::NonceAt(uint64 Number, TSuccessCallback<FBlockNonce> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).NonceAt(Number,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::NonceAt(EBlockTag Tag, TSuccessCallback<FBlockNonce> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).NonceAt(Tag,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::SendRawTransaction(FString Data, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).SendRawTransaction(Data,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::ChainId(TSuccessCallback<uint64> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).ChainId(OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::Call(FContractCall ContractCall, uint64 Number, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).Call(ContractCall,Number,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::Call(FContractCall ContractCall, EBlockTag Number, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).Call(ContractCall,Number,OnSuccess,OnFailure);
}

void SequenceAPI::FSequenceWallet::NonViewCall(FEthTransaction transaction, FPrivateKey PrivateKey, int ChainID, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure)
{
	Provider::Provider(this->ProviderUrl).NonViewCall(transaction, PrivateKey, ChainID, OnSuccess, OnFailure);
}