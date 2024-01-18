
#include "Sequence/SequenceAPI.h"

#include "Eth/Crypto.h"
#include "Http.h"
#include "HttpManager.h"
#include "RequestHandler.h"
#include "Indexer/IndexerSupport.h"
#include "Util/JsonBuilder.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"

FString SequenceAPI::SortOrderToString(ESortOrder SortOrder)
{
	switch(SortOrder)
	{
	case SequenceAPI::ASC:
		return "ASC";
	case SequenceAPI::DESC:
		return "DESC";
	default: return "";
	}
}

SequenceAPI::ESortOrder SequenceAPI::StringToSortOrder(FString String)
{
	return String == "ASC" ? ASC : DESC;
}

FString SequenceAPI::FSortBy::ToJson()
{
	return FJsonBuilder().ToPtr()
		->AddString("column", Column)
		->AddString("order", SortOrderToString(Order))
		->ToString();
}

SequenceAPI::FSortBy SequenceAPI::FSortBy::From(TSharedPtr<FJsonObject> Json)
{
	FSortBy Sort = FSortBy{};
	Sort.Column = Json->GetStringField("column");
	Sort.Order = StringToSortOrder(Json->GetStringField("order"));
	return Sort;
}

FString SequenceAPI::FPage::ToJson()
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
			Array.AddValue(SortBy.ToJson());
		}
	}
	
	return Json.ToString();
}

SequenceAPI::FPage SequenceAPI::FPage::From(TSharedPtr<FJsonObject> Json)
{
	FPage page = FPage{};

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
			Array.Push(FSortBy::From(JsonVal->AsObject()));
		}
		
		page.Sort = Array;
	}

	return page;
}

SequenceAPI::FTransaction SequenceAPI::FTransaction::Convert(FTransaction_FE Transaction_Fe)
{
	return FTransaction{
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

const FString SequenceAPI::FTransaction::ToJson()
{
	FJsonBuilder Json = FJsonBuilder();

	Json.AddInt("chainId", ChainId);
	Json.AddString("from", "0x" + From.ToHex());
	Json.AddString("to", "0x" + To.ToHex());

	if(this->Value.IsSet()) Json.AddString("value", this->Value.GetValue());

	return Json.ToString();
}

const SequenceAPI::TransactionID SequenceAPI::FTransaction::ID()
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
			->WithHeader("Authorization", AuthToken)
			->WithVerb("POST")
			->WithContentAsString(Content)
			->ProcessAndThen(OnSuccess, OnFailure);
}

//Pass in credentials in our constructor!
SequenceAPI::FSequenceWallet::FSequenceWallet()
{
	this->Indexer = NewObject<UIndexer>();
}

SequenceAPI::FSequenceWallet::FSequenceWallet(FCredentials_BE CredentialsIn)
{
	this->Credentials = CredentialsIn;
	this->Indexer = NewObject<UIndexer>();
	this->AuthToken = "Bearer " + this->Credentials.GetIDToken();
}

FString SequenceAPI::FSequenceWallet::GetWalletAddress()
{
	return this->Credentials.GetWalletAddress();
}

void SequenceAPI::FSequenceWallet::CreateWallet(TSuccessCallback<FAddress> OnSuccess,
                                                FFailureCallback OnFailure)
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

	FString Content = FJsonBuilder().ToPtr()
		->ToString();
	
	this->SendRPCAndExtract(Url("CreateWallet"), Content, OnSuccess, ExtractAddress,
	OnFailure);
}

void SequenceAPI::FSequenceWallet::GetWalletAddress(TSuccessCallback<FAddress> OnSuccess,
	FFailureCallback OnFailure)
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

	FString Content = FJsonBuilder().ToPtr()
		->ToString();
	
	this->SendRPCAndExtract(Url("GetWalletAddress"), Content, OnSuccess, ExtractAddress,
	OnFailure);
}

void SequenceAPI::FSequenceWallet::DeployWallet(uint64 ChainId,
	TSuccessCallback<FDeployWalletReturn> OnSuccess, FFailureCallback OnFailure)
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

void SequenceAPI::FSequenceWallet::Wallets(FPage Page, TSuccessCallback<FWalletsReturn> OnSuccess,
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
			
			ReturnVal = MakeValue(FWalletsReturn{Wallets, FPage::From(PageJson)});
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
	this->Wallets(FPage{}, OnSuccess, OnFailure);
}

void SequenceAPI::FSequenceWallet::SignMessage(uint64 ChainId, FAddress AccountAddress, FUnsizedData Message,
                                               const TSuccessCallback<FSignature> OnSuccess, const FFailureCallback OnFailure)
{
	const TFunction<TResult<FSignature> (FString)> ExtractSignature = [=](FString Content)
	{
		
		TSharedPtr<FJsonObject> Json = Parse(Content);
		TResult<FSignature> ReturnVal = MakeValue(FUnsizedData::Empty());

		if(!Json)
		{
			ReturnVal = MakeError(FSequenceError{RequestFail, "Json did not parse"});
		}
		else
		{
			const FString SignatureString = Json->GetStringField("signature");
			ReturnVal = MakeValue(HexStringToBinary(SignatureString));
		}
		
		return ReturnVal;
	};

	const FString Content = FJsonBuilder().ToPtr()
          ->AddInt("chainId", ChainId)
          ->AddString("accountAddress", "0x" + AccountAddress.ToHex())
          ->AddString("message", "0x" +Message.ToHex())
          ->ToString();
	
	this->SendRPCAndExtract(Url("SignMessage"), Content, OnSuccess, ExtractSignature,
	OnFailure);
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

void SequenceAPI::FSequenceWallet::SendTransaction(FTransaction Transaction, TSuccessCallback<FHash256> OnSuccess,
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
	
	this->SendRPCAndExtract(Url("SendTransaction"), Content, OnSuccess, ExtractSignature,
	OnFailure);
}

void SequenceAPI::FSequenceWallet::SendTransactionBatch(TArray<FTransaction> Transactions,
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
	for(FTransaction Transaction : Transactions)
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
	FTransaction Converted = FTransaction::Convert(Transaction);
	FString ID = Converted.ID();
	SendTransaction(Converted, [=](FHash256 Hash)
	{
		OnSuccess(ID);
	}, [=](FSequenceError Error)
	{
		OnFailure(ID, Error);
	});
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