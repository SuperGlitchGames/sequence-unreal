// Fill out your copyright notice in the Description page of Project Settings.


#include "GeneralTesting.h"
#include "Indexer/IndexerTests.h"
#include "ObjectHandler.h"
#include "Misc/AES.h"
#include "Containers/UnrealString.h"
#include "Util/HexUtility.h"
#include "Indexer/IndexerSupport.h"
#include "Auth.h"
#include "SequenceEncryptor.h"
#include "SystemDataBuilder.h"
#include "Sequence/SequenceAPI.h"
#include "tests/ContractTest.h"
#include "Tests/TestSequenceAPI.h"
#include "AES/aes.c"
#include "AES/aes.h"
#include "Authenticator.h"

// Sets default values
AGeneralTesting::AGeneralTesting()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	testingURLs.Add("https://assets.coingecko.com/coins/images/6319/large/USD_Coin_icon.png?1547042389");
	testingURLs.Add("https://www.circle.com/hubfs/share-USDC.png#keepProtocol");
	testingURLs.Add("https://assets.skyweaver.net/i7FuksL3/webapp/cards/full-cards/4x/0-silver.png");
	testingURLs.Add("https://skyweaver.net/images/skyweavercover.jpg");
}

// Called when the game starts or when spawned
void AGeneralTesting::BeginPlay()
{
	Super::BeginPlay();
}

void AGeneralTesting::TestProvider() const
{
	const TFunction<void (FString)> OnSuccess = [this](FString State)
	{
		CallbackPassed(State);
	};

	const TFunction<void (FString, FSequenceError)> OnFailure = [this](FString Data, FSequenceError Err)
	{
		CallbackFailed(Data, Err);
	};

	ContractTest::RunTest(OnSuccess, OnFailure);
	SequenceAPITest::RunTest(OnSuccess, OnFailure);
}

void AGeneralTesting::TestIndexer()
{
	TFunction<void(FString)> OnSuccess = [this](FString State)
	{
		CallbackPassed(State);
	};

	TFunction<void(FString, FSequenceError)> OnFailure = [this](FString data, FSequenceError Err)
	{
		CallbackFailed(data, Err);
	};

	IndexerTest(OnSuccess, OnFailure);
}

void AGeneralTesting::TestEncryption() const
{
	//UAuth* Auth = NewObject<UAuth>();
	//const FStoredAuthState_BE TestingStruct;

	//const FString PreEncrypt = UIndexerSupport::structToSimpleString<FStoredAuthState_BE>(TestingStruct);

	//Auth->SetNewSecureStorableAuth(TestingStruct);
	//const FSecureKey DuringEncryptStruct = Auth->GetSecureStorableAuth();

	//const FString EncryptedData = UIndexerSupport::structToSimpleString<FSecureKey>(DuringEncryptStruct);

	//Auth->SetSecureStorableAuth(DuringEncryptStruct);

	//const FString DecryptedData = UIndexerSupport::structToSimpleString<FStoredAuthState_BE>(Auth->auth);

	//UAuth* Auth = NewObject<UAuth>();
	//const FString PreEncrypt = "testing text";
	//const FString EncryptedData = USequenceEncryptor::Encrypt(PreEncrypt);
	//const FString DecryptedData = USequenceEncryptor::Decrypt(EncryptedData,PreEncrypt.Len());

	//UE_LOG(LogTemp, Display, TEXT("Pre Encrypt: %s"), *PreEncrypt);
	//UE_LOG(LogTemp, Display, TEXT("Encrypted: %s"), *EncryptedData);
	//UE_LOG(LogTemp, Display, TEXT("Post Encrypt: %s"), *DecryptedData);

	//TODO need to setup PKCS7 padding
	//AES_ctx ctx;
	//struct AES_ctx * PtrCtx = &ctx;

	//const int32 keySize = 32;
	//uint8_t key[keySize];
	//uint8_t* PtrKey = &key[0];

	//const int32 IVSize = 16;
	//uint8_t iv[IVSize];
	//uint8_t* PtrIV = &iv[0];
	//

	////setup the key
	//for (int i = 0; i < keySize; i++)
	//{
	//	key[i] = i % 16;
	//}

	//for (int i = 0; i < IVSize; i++)
	//{
	//	iv[i] = i;
	//}
	//
	//FString testData = "abcdabcdabcdabcd";
	//int32 cachedLen = testData.Len();
	//uint8_t* PtrString;
	//const int32 buffSize = 32;
	//uint8_t buff[buffSize];
	//PtrString = &buff[0];

	//UE_LOG(LogTemp, Display, TEXT("Pre Encrypted Data: %s"), *testData);

	//StringToBytes(testData,PtrString, buffSize);

	//AES_init_ctx_iv(PtrCtx, PtrKey,PtrIV);//init then use
	//AES_CBC_encrypt_buffer(PtrCtx, PtrString, buffSize);

	//FString DuringEncrypt = BytesToString(PtrString, buffSize);
	//UE_LOG(LogTemp, Display, TEXT("Encrypted Data: %s"), *DuringEncrypt);

	//AES_init_ctx_iv(PtrCtx, PtrKey, PtrIV);//init then use, need to re init this prior to decryption
	//AES_CBC_decrypt_buffer(PtrCtx, PtrString, buffSize);

	//testData = BytesToString(PtrString, buffSize);

	//testData = testData.Left(cachedLen);

	//UE_LOG(LogTemp, Display, TEXT("Pst Encrypted Data: %s"), *testData);



	//int32 bytes = 0;

	//for (auto c : testData.GetCharArray())
	//{
	//	bytes += sizeof(c);
	//}

	//UE_LOG(LogTemp, Display, TEXT("Size: %d"), bytes);
}

//dedicated encryption test!

void AGeneralTesting::TestMisc()
{//used for testing various things in the engine to verify behaviour
	imgHandler = NewObject<UObjectHandler>();
	imgHandler->setup(true);//we want to test caching!
	imgHandler->FOnDoneImageProcessingDelegate.BindUFunction(this, "OnDoneImageProcessing");
	imgHandler->requestImages(this->testingURLs);
}

void AGeneralTesting::OnDoneImageProcessing()
{//forward this to the front as we will be able to view all the images from there!
	this->testMiscForwarder(this->imgHandler->getProcessedImages());
}

void AGeneralTesting::TestSequence() const
{
	TFunction<void(FString)> OnSuccess = [this](FString State)
	{
		CallbackPassed(State);
	};

	TFunction<void(FString, FSequenceError)> OnFailure = [this](FString data, FSequenceError Err)
	{
		CallbackFailed(data, Err);
	};

	TestSequenceData(OnSuccess, OnFailure);
}

void AGeneralTesting::testSystemDataBuilder()
{//testing system data builder
	USystemDataBuilder* sysBuilder = NewObject<USystemDataBuilder>();
	SequenceAPI::FSequenceWallet* wallet = new SequenceAPI::FSequenceWallet();
	sysBuilder->testGOTokenData(wallet,137, "0x0E0f9d1c4BeF9f0B8a2D9D4c09529F260C7758A2");
}

// Called every frame
void AGeneralTesting::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AGeneralTesting::CallbackPassed(FString StateData) const
{
	UE_LOG(LogTemp, Display, TEXT("========================================================================="));
	UE_LOG(LogTemp, Display, TEXT("[Callback Passed!]\nAdditional State: [%s]"), *StateData);
	UE_LOG(LogTemp, Display, TEXT("========================================================================="));
}

void AGeneralTesting::CallbackFailed(const FString StateData, FSequenceError Error) const
{
	UE_LOG(LogTemp, Display, TEXT("========================================================================="));
	UE_LOG(LogTemp, Error, TEXT("[Callback Failed!]\nAdditional State: [%s]"), *StateData);
	UE_LOG(LogTemp, Error, TEXT("[Error Message]:\n[%s]"),*Error.Message);
	UE_LOG(LogTemp, Error, TEXT("[Error Type]: [%s]"),*ErrorToString(Error.Type));
	UE_LOG(LogTemp, Display, TEXT("========================================================================="));
}

FString AGeneralTesting::ErrorToString(EErrorType Error)
{
	switch (Error) {
	case NotFound:
		return "NotFound";
	case ResponseParseError:
		return "ResponseParseError";
	case EmptyResponse:
		return "EmptyResponse";
	case UnsupportedMethodOnChain:
		return "UnsupportedMethodOnChain";
	case RequestFail:
		return "RequestFail";
	case RequestTimeExceeded:
		return "RequestTimeExceeded";
	case TestFail:
		return "TestFail";
	default:
		return "SequenceError";
	}
}