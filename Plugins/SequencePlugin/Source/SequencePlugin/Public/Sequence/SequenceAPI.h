#pragma once
#include "Async.h"
#include "EthTransaction.h"
#include "RPCCaller.h"
#include "Types/BinaryData.h"

namespace SequenceAPI
{
	using FSignature = FUnsizedData;

	enum ESortOrder {
		DESC,
		ASC
	};

	FString SortOrderToString(ESortOrder SortOrder);
	ESortOrder StringToSortOrder(FString String);

	struct FSortBy
	{
		FString Column;
		ESortOrder Order;

		FString ToJson();
		static FSortBy From(TSharedPtr<FJsonObject> Json);
	};

	struct FPage
	{
		TOptional<uint64> PageSize;
		TOptional<uint64> PageNum;
		TOptional<uint64> TotalRecords;
		TOptional<FString> Column;
		TOptional<TArray<FSortBy>> Sort;

		FString ToJson();
		static FPage From(TSharedPtr<FJsonObject> Json);
	};

	struct FTransaction
	{
		uint64 ChainId;
		FAddress From;
		FAddress To;
		TOptional<FString> AutoGas;
		TOptional<uint64> Nonce;
		TOptional<FString> Value;
		TOptional<FString> CallData;
		TOptional<FString> TokenAddress;
		TOptional<FString> TokenAmount;
		TOptional<TArray<FString>> TokenIds;
		TOptional<TArray<FString>> TokenAmounts;

		FString ToJson();
	};

	struct FPartnerWallet
	{
		uint64 Number;
		uint64 PartnerId;
		uint64 WalletIndex;
		FString WalletAddress;

		static FPartnerWallet From(TSharedPtr<FJsonObject> Json);
	};

	struct FDeployWalletReturn
	{
		FString Address;
		FString TransactionHash;
	};

	struct FWalletsReturn
	{
		TArray<FPartnerWallet> Wallets;
		FPage Page;
	};

	class FSequenceWallet : public RPCCaller
	{
		FString AuthToken = "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJwYXJ0bmVyX2lkIjoyLCJ3YWxsZXQiOiIweDY2MDI1MDczNGYzMTY0NDY4MWFlMzJkMDViZDdlOGUyOWZlYTI5ZTEifQ.FC8WmaC_hW4svdrs4rxyKcvoekfVYFkFFvGwUOXzcHA";
		const FString Hostname = "https://next-api.sequence.app";
		const FString Path = "/rpc/Wallet/";
		FString Url(FString Name) const;
		virtual void SendRPC(FString Url, FString Content, TSuccessCallback<FString> OnSuccess, FFailureCallback OnFailure) override;
		
	public:
		FSequenceWallet(FString Hostname);
		FSequenceWallet();
		
		void CreateWallet(uint64 AccountIndex, TSuccessCallback<FAddress> OnSuccess, FFailureCallback OnFailure);
		void GetWalletAddress(uint64 AccountIndex, TSuccessCallback<FAddress> OnSuccess, FFailureCallback OnFailure);
		void DeployWallet(uint64 ChainId, uint64 AccountIndex, TSuccessCallback<FDeployWalletReturn> OnSuccess, FFailureCallback OnFailure);
		void Wallets(FPage Page, TSuccessCallback<FWalletsReturn> OnSuccess, FFailureCallback OnFailure);
		void Wallets(TSuccessCallback<FWalletsReturn> OnSuccess, FFailureCallback OnFailure);
		void SignMessage(uint64 ChainId, FAddress AccountAddress, FUnsizedData Message, TSuccessCallback<FSignature> OnSuccess, FFailureCallback OnFailure);
		void IsValidMessageSignature(uint64 ChainId, FAddress WalletAddress, FUnsizedData Message, FSignature Signature, TSuccessCallback<bool> OnSuccess, FFailureCallback OnFailure);
		void SendTransaction(FTransaction Transaction, TSuccessCallback<FHash256> OnSuccess, FFailureCallback OnFailure);
		void SendTransactionBatch(TArray<FTransaction> Transactions, TSuccessCallback<FHash256> OnSuccess, FFailureCallback OnFailure);
	};

}