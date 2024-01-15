#pragma once
#include "BinaryData.h"
#include "Eth/EthTransaction.h"
#include "Eth/Crypto.h"

class FWallet
{
private:
	FPrivateKey PrivateKey;
	FPublicKey PublicKey;
	FAddress Address;
public:
	/*
	* Generates a random wallet
	*/
	FWallet();

	~FWallet();

	/*
	* Generates a wallet based on a given private key
	*/
	FWallet(FPrivateKey PrivateKey);

	/*
	* Generates a wallet based on a given private key in string form
	*/
	FWallet(const FString& PrivateKey);

	/*
	* Signs the given message
	*/
	TArray<uint8> SignMessage(FString message);

	//Accessors//
	FPrivateKey GetWalletPrivateKey();
	FPublicKey GetWalletPublicKey();
	FAddress GetWalletAddress();
	//Accessors//

	//Used to build out the nonce used during the signing process
private: TArray<uint8_t> BuildSigningNonce(uint8_t* messageHash, int32 size);
};