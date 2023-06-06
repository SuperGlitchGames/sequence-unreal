#include "Types/BinaryData.h"
#include "HexUtility.h"

// There is a decent bit of code duplication here but I don't anticipate this code to change much

uint8* BlankArray(ByteLength size)
{
	auto Arr = new uint8[size];

	for(auto i = 0; i < size; i++)
	{
		Arr[i] = 0x00;
	}

	return Arr;
}

void FBinaryData::Destroy()
{
	delete [] this->Arr;
}

FString FBinaryData::ToHex()
{
	if(Arr == nullptr)
	{
		return "";
	}
	
	return HashToHexString(GetLength(), Arr);
}

void FBinaryData::Renew()
{
	this->Arr = BlankArray(this->GetLength());
}

FNonUniformData::FNonUniformData(uint8* Arr, ByteLength Length) : Length(Length)
{
	this->Arr = Arr;
}

FNonUniformData FNonUniformData::Copy()
{
	auto NewArr = new uint8[Length];
	for(auto i = 0; i < Length; i++) NewArr[i] = Arr[i];
	return FNonUniformData{NewArr, Length};
}

const ByteLength FNonUniformData::GetLength()
{
	return this->Length;
}

FNonUniformData FNonUniformData::Trim()
{
	auto trimmed = Trimmed(*this);
	delete [] this->Arr;
	this->Arr = trimmed.Arr;
	return trimmed;
}


FNonUniformData Trimmed(FBinaryData& Data)
{
	auto empty_count = 0;
	auto len = Data.GetLength();

	for(auto i = 0; i < len; i++)
	{
		if(Data.Arr[i] == 0x00)
		{
			empty_count += 1;
		}
		else
		{
			break;
		}
	}

	ByteLength NewLength = Data.GetLength() - empty_count;

	if(NewLength == 0)
	{
		return FNonUniformData{new uint8[1]{0x00}, 1};
	}
	
	const auto NewData = new uint8[NewLength];

	for(auto i = 0; i < NewLength; i++)
	{
		NewData[i] = Data.Arr[i + empty_count];
	}

	return FNonUniformData(NewData, NewLength);
}

FHash256 FHash256::New()
{
	auto Data = FHash256{};
	Data.Renew();
	return Data;
}

FHash256 FHash256::From(uint8* Arr)
{
	auto Data = FHash256{};
	Data.Arr = Arr;
	return Data;
}

FHash256 FHash256::From(FString Str)
{
	return From(HexStringToHash(Size, Str));
}

const ByteLength FHash256::GetLength()
{
	return Size;
}

FAddress FAddress::New()
{
	auto Data = FAddress{};
	Data.Renew();
	return Data;
}

FAddress FAddress::From(uint8* Arr)
{
	auto Data = FAddress{};
	Data.Arr = Arr;
	return Data;
}

FAddress FAddress::From(FString Str)
{
	return From(HexStringToHash(Size, Str));
}

const ByteLength FAddress::GetLength()
{
	return Size;
}

FPublicKey FPublicKey::New()
{
	auto Data = FPublicKey{};
	Data.Renew();
	return Data;
}

FPublicKey FPublicKey::From(uint8* Arr)
{
	auto Data = FPublicKey{};
	Data.Arr = Arr;
	return Data;
}

FPublicKey FPublicKey::From(FString Str)
{
	return From(HexStringToHash(Size, Str));
}

const ByteLength FPublicKey::GetLength()
{
	return Size;
}

FPrivateKey FPrivateKey::New()
{
	auto Data = FPrivateKey{};
	Data.Renew();
	return Data;
}

FPrivateKey FPrivateKey::From(uint8* Arr)
{
	auto Data = FPrivateKey{};
	Data.Arr = Arr;
	return Data;
}

FPrivateKey FPrivateKey::From(FString Str)
{
	return From(HexStringToHash(Size, Str));
}

const ByteLength FPrivateKey::GetLength()
{
	return Size;
}

FBloom FBloom::New()
{
	auto Data = FBloom{};
	Data.Renew();
	return Data;
}

FBloom FBloom::From(uint8* Arr)
{
	auto Data = FBloom{};
	Data.Arr = Arr;
	return Data;
}

FBloom FBloom::From(FString Str)
{
	return From(HexStringToHash(Size, Str));
}

const ByteLength FBloom::GetLength()
{
	return Size;
}

FBlockNonce FBlockNonce::New()
{
	auto Data = FBlockNonce{};
	Data.Renew();
	return Data;
}

FBlockNonce FBlockNonce::From(uint8* Arr)
{
	auto Data = FBlockNonce{};
	Data.Arr = Arr;
	return Data;
}

FBlockNonce FBlockNonce::From(FString Str)
{
	return From(HexStringToHash(Size, Str));
}

const ByteLength FBlockNonce::GetLength()
{
	return Size;
}

void FBlockNonce::Increment()
{
	for(auto i = 0; i < GetLength(); i--)
	{
		auto index = GetLength() - 1 - i;
		this->Arr[index] += 1;

		if(this->Arr[index] != 0x00)
		{
			return;
		}
	}
}

FNonUniformData StringToUTF8(FString String)
{
	uint32 Length = String.Len();

	auto binary = FNonUniformData{
		new uint8[Length], Length
	};

	StringToBytes(String, binary.Arr, Length);

	// I have no idea why I need to add 1 but it works
	for(auto i = 0; i < binary.GetLength(); i++)
	{
		binary.Arr[i] = binary.Arr[i] + 1;
	}

	return binary;
}

FString UTF8ToString(FNonUniformData BinaryData)
{
	TArray<uint8> Buffer;
	for(auto i = 0; i < BinaryData.GetLength(); i++)
	{
		Buffer.Add(BinaryData.Arr[i]);
	}
	Buffer.Add('\0');
	return reinterpret_cast<const char*>(Buffer.GetData());
}

FNonUniformData FUniformData::Copy()
{
	auto Length = GetLength();
	auto NewArr = new uint8[Length];
	for(auto i = 0; i < Length; i++) NewArr[i] = Arr[i];
	return FNonUniformData{NewArr, Length};
}

FNonUniformData HexStringToBinary(const FString Hex)
{
	auto HexCopy = FString(Hex);
	HexCopy.RemoveFromStart("0x");
	
	const uint32 Size = (HexCopy.Len() / 2) + (HexCopy.Len() % 2);
	
	return FNonUniformData(HexStringToHash(Size, Hex), Size);
}
