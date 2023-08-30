#include "Eth/EthTransaction.h"
#include "Types/BinaryData.h"
#include "Eth/Crypto.h"
#include "Util/HexUtility.h"
#include "Eth/RLP.h"
#include "Bitcoin-Cryptography-Library/cpp/Ecdsa.hpp"
#include "Bitcoin-Cryptography-Library/cpp/Keccak256.hpp"
#include "Bitcoin-Cryptography-Library/cpp/Sha256Hash.hpp"
#include "Bitcoin-Cryptography-Library/cpp/Uint256.hpp"

FEthTransaction::FEthTransaction(FBlockNonce Nonce, FUnsizedData GasPrice, FUnsizedData GasLimit, FAddress To,
	FUnsizedData Value, FUnsizedData Data) : Nonce(Nonce), GasPrice(GasPrice), GasLimit(GasLimit), To(To),
	                                               Value(Value), Data(Data), V(nullptr, 0), R(FHash256{}), S(FHash256{})
{
}

void FEthTransaction::Sign(FPrivateKey PrivateKey, int ChainID)
{
	FString NonceString = TrimHex(Nonce.ToHex());
	UE_LOG(LogTemp, Display, TEXT("Nonce: %s"), *NonceString);

	FUnsizedData TrimmedNonce = Trimmed(Nonce);

	UE_LOG(LogTemp, Display, TEXT("My trimmed nonce is %s"), *TrimmedNonce.ToHex());
	
	FUnsizedData EncodedSigningData = RLP::Encode(Itemize(new RLPItem[]
	{
		Itemize(TrimmedNonce), // Nonce
		Itemize(GasPrice.Trim()), // GasPrice
		Itemize(GasLimit.Trim()), // GasLimit
		Itemize(Trimmed(To)), // To
		Itemize(Value), // Value
		Itemize(Data), // Data
		Itemize(HexStringToBinary(IntToHexString(ChainID))), // V
		Itemize(HexStringToBinary("")), // R 
		Itemize(HexStringToBinary("")), // S
	}, 9));

	TrimmedNonce.Destroy();

	FHash256 SigningHash = FHash256::New();
	Keccak256::getHash(EncodedSigningData.Arr, EncodedSigningData.GetLength(), SigningHash.Arr);

	FPublicKey PublicKey = GetPublicKey(PrivateKey);
	FAddress Addr = GetAddress(PublicKey);
	FHash256 MyR = FHash256::New();
	FHash256 MyS = FHash256::New();
	FHash256 MyY = FHash256::New();
	Uint256 BigR, BigS;
	uint16 RecoveryParameter;
	bool IsSuccess = Ecdsa::signWithHmacNonce(Uint256(PrivateKey.Arr), Sha256Hash(SigningHash.Arr, FHash256::Size), BigR, BigS, RecoveryParameter);
	BigR.getBigEndianBytes(MyR.Arr);
	BigS.getBigEndianBytes(MyS.Arr);
	
	const uint16 BigV = ChainID * 2 + 35 + RecoveryParameter;
	UE_LOG(LogTemp, Display, TEXT("Recovery Bit: %d"), RecoveryParameter);
	UE_LOG(LogTemp, Display, TEXT("ENCODED SIGNING DATA: %s"), *EncodedSigningData.ToHex());
	UE_LOG(LogTemp, Display, TEXT("SIGNING HASH: %s"), *SigningHash.ToHex());
	UE_LOG(LogTemp, Display, TEXT("R: %s"), *MyR.ToHex());
	UE_LOG(LogTemp, Display, TEXT("S: %s"), *MyS.ToHex());
	UE_LOG(LogTemp, Display, TEXT("V: %s"), *IntToHexString(35));
	UE_LOG(LogTemp, Display, TEXT("PRIVATE KEY: %s"), *PrivateKey.ToHex());

	this->V = HexStringToBinary(IntToHexString(BigV));
	this->R = MyR;
	this->S = MyS;
}

FHash256 FEthTransaction::GetSignedTransactionHash(FPrivateKey Key, int ChainID)
{
	FUnsizedData SignedData = GetSignedTransaction(Key, ChainID);
	FHash256 Hash = FHash256::New();
	Keccak256::getHash(SignedData.Arr, SignedData.GetLength(), Hash.Arr);
	return Hash;
}

FHash256 FEthTransaction::GetUnsignedTransactionHash(int ChainID)
{
	FString NonceString = TrimHex(Nonce.ToHex());
	UE_LOG(LogTemp, Display, TEXT("Nonce: %s"), *NonceString);

	FUnsizedData TrimmedNonce = Trimmed(Nonce);
	
	FUnsizedData EncodedSigningData = RLP::Encode(Itemize(new RLPItem[]
	{
		Itemize(TrimmedNonce), // Nonce
		Itemize(GasPrice.Trim()), // GasPrice
		Itemize(GasLimit.Trim()), // GasLimit
		Itemize(Trimmed(To)), // To
		Itemize(Value), // Value
		Itemize(Data), // Data
		Itemize(HexStringToBinary(IntToHexString(ChainID))), // V
		Itemize(HexStringToBinary("")), // R 
		Itemize(HexStringToBinary("")), // S
	}, 9));

	TrimmedNonce.Destroy();

	FHash256 SigningHash = FHash256::New();
	Keccak256::getHash(EncodedSigningData.Arr, EncodedSigningData.GetLength(), SigningHash.Arr);

	return SigningHash;
}

FUnsizedData FEthTransaction::GetSignedTransaction(FPrivateKey PrivateKey, int ChainID)
{
	this->Sign(PrivateKey, ChainID);

	const FString NonceString = TrimHex(Nonce.ToHex());
	UE_LOG(LogTemp, Display, TEXT("Nonce: %s"), *NonceString);
	
	return RLP::Encode(Itemize(new RLPItem[]
	{
		Itemize(HexStringToBinary(NonceString)), // Nonce
		Itemize(GasPrice.Trim()), // GasPrice
		Itemize(GasLimit.Trim()), // GasLimit
		Itemize(Trimmed(To)), // To
		Itemize(Value), // Value
		Itemize(Data), // Data
		Itemize(V), // V
		Itemize(R), // R 
		Itemize(S), // S
	}, 9));
}


void FEthTransaction::Log()
{
	UE_LOG(LogTemp, Display, TEXT("Nonce: %s"), *Nonce.ToHex());
	UE_LOG(LogTemp, Display, TEXT("Gasprice: %s"), *GasPrice.ToHex());
	UE_LOG(LogTemp, Display, TEXT("Gaslimit: %s"), *GasLimit.ToHex());
	UE_LOG(LogTemp, Display, TEXT("Address: %s"), *To.ToHex());
	UE_LOG(LogTemp, Display, TEXT("Value: %s"), *Value.ToHex());
	UE_LOG(LogTemp, Display, TEXT("Data: %s"), *Data.ToHex());
	UE_LOG(LogTemp, Display, TEXT("V: %s"), *V.ToHex());
	UE_LOG(LogTemp, Display, TEXT("R: %s"), *R.ToHex());
	UE_LOG(LogTemp, Display, TEXT("S: %s"), *S.ToHex());
}