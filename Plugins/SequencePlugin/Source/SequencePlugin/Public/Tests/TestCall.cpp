#include "Crypto.h"
#include "Types/BinaryData.h"
#include "FEthTransaction.h"
#include "Provider.h"
#include "Misc/AutomationTest.h"
#include "Types/ContractCall.h"
#include "ABI/ABI.h"
#include "Bitcoin-Cryptography-Library/cpp/Keccak256.hpp"
#include "ABI/ABITypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestCall, "Public.Tests.TestCall",
                                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool TestCall::RunTest(const FString& Parameters)
{
	auto provider = Provider("http://localhost:3000/");


	auto res = provider.BlockByNumber(EBlockTag::Latest);

	/*FBlockNonce NONCE;

	if(res.HasValue())
	{
		NONCE = FBlockNonce::From(res.GetValue()->GetStringField("nonce"));
		UE_LOG(LogTemp, Display, TEXT("Before %s"), *NONCE.ToHex());
		NONCE.Increment();
		UE_LOG(LogTemp, Display, TEXT("After %s"), *NONCE.ToHex());
	}*/

	auto NONCE = FBlockNonce::From("0x00");

	UE_LOG(LogTemp, Display, TEXT("My NONCE value is %s"), *NONCE.ToHex());
	
	// constants for the transaction
	
	auto GASPRICE= HexStringToBinary("3b9aca00").Trim(); // "0x4A817C800";
	auto GASLIMIT = HexStringToBinary("0f4240").Trim();
	auto TO = FAddress::From(""); // 0000000000000000000000000000000000000000
	auto VALUE = HexStringToBinary(""); // 0xDE0B6B3A7640000;
	auto PRIVATE_KEY = FPrivateKey::From("abc0000000000000000000000000000000000000000000000000000000000001"); //("dc04c5399f82306ec4b4d654a342f40e2e0620fe39950d967e1e574b32d4dd36");
	auto CHAIN_ID = 1337;
	auto small_byte_code = "0x600180600b6000396000f3";
	auto vm_byte_code = "0x608060405234801561001057600080fd5b50336000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff1602179055506064600160003073ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200190815260200160002081905550610792806100a56000396000f3fe60806040526004361061003f5760003560e01c80638da5cb5b14610044578063ca9d07ba1461006f578063e18a7b9214610098578063efef39a1146100d5575b600080fd5b34801561005057600080fd5b506100596100f1565b60405161006691906103dc565b60405180910390f35b34801561007b57600080fd5b5061009660048036038101906100919190610432565b610115565b005b3480156100a457600080fd5b506100bf60048036038101906100ba919061048b565b6101fc565b6040516100cc91906104c7565b60405180910390f35b6100ef60048036038101906100ea9190610432565b610214565b005b60008054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b60008054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff163373ffffffffffffffffffffffffffffffffffffffff16146101a3576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161019a9061053f565b60405180910390fd5b80600160003073ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200190815260200160002060008282546101f2919061058e565b9250508190555050565b60016020528060005260406000206000915090505481565b670de0b6b3a76400008161022891906105c2565b34101561026a576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161026190610676565b60405180910390fd5b80600160003073ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff1681526020019081526020016000205410156102ec576040517f08c379a00000000000000000000000000000000000000000000000000000000081526004016102e390610708565b60405180910390fd5b80600160003073ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff168152602001908152602001600020600082825461033b9190610728565b9250508190555080600160003373ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff1681526020019081526020016000206000828254610391919061058e565b9250508190555050565b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b60006103c68261039b565b9050919050565b6103d6816103bb565b82525050565b60006020820190506103f160008301846103cd565b92915050565b600080fd5b6000819050919050565b61040f816103fc565b811461041a57600080fd5b50565b60008135905061042c81610406565b92915050565b600060208284031215610448576104476103f7565b5b60006104568482850161041d565b91505092915050565b610468816103bb565b811461047357600080fd5b50565b6000813590506104858161045f565b92915050565b6000602082840312156104a1576104a06103f7565b5b60006104af84828501610476565b91505092915050565b6104c1816103fc565b82525050565b60006020820190506104dc60008301846104b8565b92915050565b600082825260208201905092915050565b7f4f6e6c7920746865206f776e65722063616e20726566696c6c2e000000000000600082015250565b6000610529601a836104e2565b9150610534826104f3565b602082019050919050565b600060208201905081810360008301526105588161051c565b9050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b6000610599826103fc565b91506105a4836103fc565b92508282019050808211156105bc576105bb61055f565b5b92915050565b60006105cd826103fc565b91506105d8836103fc565b92508282026105e6816103fc565b915082820484148315176105fd576105fc61055f565b5b5092915050565b7f596f75206d75737420706179206174206c65617374203120455448207065722060008201527f63757063616b6500000000000000000000000000000000000000000000000000602082015250565b60006106606027836104e2565b915061066b82610604565b604082019050919050565b6000602082019050818103600083015261068f81610653565b9050919050565b7f4e6f7420656e6f7567682063757063616b657320696e2073746f636b20746f2060008201527f636f6d706c657465207468697320707572636861736500000000000000000000602082015250565b60006106f26036836104e2565b91506106fd82610696565b604082019050919050565b60006020820190508181036000830152610721816106e5565b9050919050565b6000610733826103fc565b915061073e836103fc565b92508282039050818111156107565761075561055f565b5b9291505056fea26469706673582212200e672b8a79d676983fe49168988d8ca0df2a313d4545d588cc2399be5edb246964736f6c63430008120033";
	auto byte_code = "0x608060405234801561001057600080fd5b50610997806100206000396000f3fe608060405234801561001057600080fd5b50600436106100be5760003560e01c80633950935111610076578063a457c2d71161005b578063a457c2d7146102f0578063a9059cbb14610329578063dd62ed3e14610362576100be565b8063395093511461028457806370a08231146102bd576100be565b806323b872dd116100a757806323b872dd1461012a5780632e72102f1461016d578063378934b41461024b576100be565b8063095ea7b3146100c357806318160ddd14610110575b600080fd5b6100fc600480360360408110156100d957600080fd5b5073ffffffffffffffffffffffffffffffffffffffff813516906020013561039d565b604080519115158252519081900360200190f35b6101186103b3565b60408051918252519081900360200190f35b6100fc6004803603606081101561014057600080fd5b5073ffffffffffffffffffffffffffffffffffffffff8135811691602081013590911690604001356103b9565b6102496004803603606081101561018357600080fd5b81019060208101813564010000000081111561019e57600080fd5b8201836020820111156101b057600080fd5b803590602001918460208302840111640100000000831117156101d257600080fd5b9193909273ffffffffffffffffffffffffffffffffffffffff8335169260408101906020013564010000000081111561020a57600080fd5b82018360208201111561021c57600080fd5b8035906020019184602083028401116401000000008311171561023e57600080fd5b509092509050610417565b005b6102496004803603604081101561026157600080fd5b5073ffffffffffffffffffffffffffffffffffffffff8135169060200135610509565b6100fc6004803603604081101561029a57600080fd5b5073ffffffffffffffffffffffffffffffffffffffff8135169060200135610517565b610118600480360360208110156102d357600080fd5b503573ffffffffffffffffffffffffffffffffffffffff1661055a565b6100fc6004803603604081101561030657600080fd5b5073ffffffffffffffffffffffffffffffffffffffff8135169060200135610582565b6100fc6004803603604081101561033f57600080fd5b5073ffffffffffffffffffffffffffffffffffffffff81351690602001356105c5565b6101186004803603604081101561037857600080fd5b5073ffffffffffffffffffffffffffffffffffffffff813581169160200135166105d2565b60006103aa33848461060a565b50600192915050565b60025490565b60006103c68484846106b9565b73ffffffffffffffffffffffffffffffffffffffff841660009081526001602090815260408083203380855292529091205461040d91869161040890866107ac565b61060a565b5060019392505050565b60005b818110156105015785858281811061042e57fe5b9050602002013573ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff1663a9059cbb8585858581811061047357fe5b905060200201356040518363ffffffff1660e01b8152600401808373ffffffffffffffffffffffffffffffffffffffff16815260200182815260200192505050602060405180830381600087803b1580156104cd57600080fd5b505af11580156104e1573d6000803e3d6000fd5b505050506040513d60208110156104f757600080fd5b505060010161041a565b505050505050565b6105138282610823565b5050565b33600081815260016020908152604080832073ffffffffffffffffffffffffffffffffffffffff8716845290915281205490916103aa91859061040890866108e6565b73ffffffffffffffffffffffffffffffffffffffff1660009081526020819052604090205490565b33600081815260016020908152604080832073ffffffffffffffffffffffffffffffffffffffff8716845290915281205490916103aa91859061040890866107ac565b60006103aa3384846106b9565b73ffffffffffffffffffffffffffffffffffffffff918216600090815260016020908152604080832093909416825291909152205490565b73ffffffffffffffffffffffffffffffffffffffff821661062a57600080fd5b73ffffffffffffffffffffffffffffffffffffffff831661064a57600080fd5b73ffffffffffffffffffffffffffffffffffffffff808416600081815260016020908152604080832094871680845294825291829020859055815185815291517f8c5be1e5ebec7d5bd14f71427d1e84f3dd0314c0f7b2291e5b200ac8c7c3b9259281900390910190a3505050565b73ffffffffffffffffffffffffffffffffffffffff82166106d957600080fd5b73ffffffffffffffffffffffffffffffffffffffff831660009081526020819052604090205461070990826107ac565b73ffffffffffffffffffffffffffffffffffffffff808516600090815260208190526040808220939093559084168152205461074590826108e6565b73ffffffffffffffffffffffffffffffffffffffff8084166000818152602081815260409182902094909455805185815290519193928716927fddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3ef92918290030190a3505050565b60008282111561081d57604080517f08c379a000000000000000000000000000000000000000000000000000000000815260206004820152601760248201527f536166654d617468237375623a20554e444552464c4f57000000000000000000604482015290519081900360640190fd5b50900390565b73ffffffffffffffffffffffffffffffffffffffff821661084357600080fd5b60025461085090826108e6565b60025573ffffffffffffffffffffffffffffffffffffffff821660009081526020819052604090205461088390826108e6565b73ffffffffffffffffffffffffffffffffffffffff83166000818152602081815260408083209490945583518581529351929391927fddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3ef9281900390910190a35050565b60008282018381101561095a57604080517f08c379a000000000000000000000000000000000000000000000000000000000815260206004820152601660248201527f536166654d617468236164643a204f564552464c4f5700000000000000000000604482015290519081900360640190fd5b939250505056fea26469706673582212201e8282683b3b9580e5722aa29d3e976acdd4cd35c3e88cdd1abb688867d0547a64736f6c63430007040033";
	auto DATA = HexStringToBinary(vm_byte_code);
	//auto deployedAddress = provider.DeployContract(vm_byte_code, PRIVATE_KEY, CHAIN_ID);
	//UE_LOG(LogTemp, Display, TEXT("The contract address is %s"), *(deployedAddress.ToHex()));
	
	auto transaction = FEthTransaction{
		NONCE,
		GASPRICE,
		GASLIMIT,
		TO,
		VALUE,
		DATA,
	};

	UE_LOG(LogTemp, Display, TEXT("Transaction Hash:"), *transaction.GetTransactionHash(CHAIN_ID).ToHex());

	//get gas price nonce and gas amount
	TResult<uint64> val =  provider.TransactionCount(FAddress::From("c683a014955b75F5ECF991d4502427c8fa1Aa249"), EBlockTag::Latest).Get();
	UE_LOG(LogTemp, Display, TEXT("Nonce value: %llu"), val.GetValue());
	auto price = provider.GetGasPrice().Get();
	UE_LOG(LogTemp, Display, TEXT("Gas price is: %s"), *price.GetValue().ToHex());
	auto deploymentGas = provider.EstimateDeploymentGas(FAddress::From("c683a014955b75F5ECF991d4502427c8fa1Aa249"), FString(small_byte_code)).Get();
	UE_LOG(LogTemp, Display, TEXT("Deployment gas: %s"), *deploymentGas.GetValue().ToHex());


	
	auto SignedTransaction = transaction
		.GetSignedTransaction(PRIVATE_KEY, CHAIN_ID);
	provider.SendRawTransaction(
		"0x" + SignedTransaction.ToHex()
	 );
	
	auto publicKey = GetPublicKey(PRIVATE_KEY);
	auto from = GetAddress(publicKey);
	auto contractAddress = FAddress::From("0x81b35475847c79ce03e9ce407d93a40475b6a166");

	auto chain_id = provider.ChainId().Get().GetValue();
	auto TransactionHash = GetKeccakHash(SignedTransaction);

	UE_LOG(LogTemp, Display, TEXT("From: %s"), *from.ToHex());


	//TODO: remove this
	const FString stringArg = "0x81b35475847c79ce03e9ce407d93a40475b6a166";
	auto arg = FABIArg::New(stringArg);
	auto Obj = ABI::EncodeArgs("cupcakeBalances(address)", new FABIArg[1]{arg}, 1);

	auto Signature = FHash256::New();
	auto Msg = StringToUTF8("cupcakeBalances(address)");
	Keccak256::getHash(Msg.Arr, Msg.GetLength(), Signature.Arr);

	auto mAbiEncoder = ABI();
	auto arg1 = FABIUInt32Property(3);
	TArray<FABIProperty*> Properties{&arg1};
	auto contractData = ABI::Encode("purchase(uint256)", Properties);
	
	auto contractNonce = FBlockNonce::From("0x01");
	auto contractGasPrice= HexStringToBinary("3b9aca00").Trim(); // "0x4A817C800";
	auto contractGasLimit = HexStringToBinary("0f4240").Trim();
	auto contractTo = FAddress::From("0x78cfa0a45e279bfe8add6aeae31d9993ec08b66b");
	auto contractValue = HexStringToBinary("0x29a2241af62c0000");



	//estimate gas needed
	
	auto mContractCall = ContractCall{
		TOptional<FAddress>(FAddress::From("c683a014955b75F5ECF991d4502427c8fa1Aa249")),
		FAddress::From("0x78cfa0a45e279bfe8add6aeae31d9993ec08b66b"),
		TOptional<uint64>(),
		TOptional<uint64>(),
		TOptional<uint64>(3000000000000000000),
		TOptional<FString>(contractData.ToHex()),
	};
	auto contractCallGas = provider.EstimateContractCallGas(mContractCall).Get();
	UE_LOG(LogTemp, Display, TEXT("Method call gas: %s"), *contractCallGas.GetValue().ToHex());
	

	//call purchase function in smart contract
	auto contractTransaction = FEthTransaction{
		contractNonce,
		contractGasPrice,
		contractGasLimit,
		contractTo,
		contractValue,
		contractData,
	};
	provider.NonViewCall(contractTransaction, PRIVATE_KEY, CHAIN_ID);

	//refill

	auto arg2 = FABIUInt32Property(5);
	TArray<FABIProperty*> Properties2{&arg2};
	auto contractData2 = ABI::Encode("refill(uint256)", Properties2);
	
	auto contractNonce2 = FBlockNonce::From("0x02");
	auto contractGasPrice2= HexStringToBinary("3b9aca00").Trim(); // "0x4A817C800";
	auto contractGasLimit2 = HexStringToBinary("0f4240").Trim();
	auto contractTo2 = FAddress::From("0x78cfa0a45e279bfe8add6aeae31d9993ec08b66b");
	auto contractValue2 = HexStringToBinary("0x00");

	//call purchase function in smart contract
	auto contractTransaction2 = FEthTransaction{
		contractNonce2,
		contractGasPrice2,
		contractGasLimit2,
		contractTo2,
		contractValue2,
		contractData2,
	};
	provider.NonViewCall(contractTransaction2, PRIVATE_KEY, CHAIN_ID);
	


	
	//check if balance decrease for 
	/*
	
	auto mArgs = TArray<MyProperty*>();
	AddressProperty myAddressProperty = AddressProperty("0x81b35475847c79ce03e9ce407d93a40475b6a166");
	mArgs.Add(&myAddressProperty);
	auto encodedData = mAbiEncoder.MyEncode("cupcakeBalances(address)", mArgs);
	UE_LOG(LogTemp, Display, TEXT("Final encoding data: %s"), *encodedData);
	
	provider.Call(ContractCall{
		TOptional<FAddress>(),
		contractAddress,
		TOptional<uint64>(),
		TOptional<uint64>(),
		TOptional<uint64>(),
		TOptional<FString>(encodedData),
	}, EBlockTag::Latest);

	*/

	/* testing contract calls remove after*/

	/*
	auto abiEncoder = ABI();
	auto args = TArray<MyProperty*>();
	UE_LOG(LogTemp, Display, TEXT("Encode2 method: %s"), *encodedMethod);
	AddressProperty mAddressProperty = AddressProperty("0x81b35475847c79ce03e9ce407d93a40475b6a166");
	BooleanProperty mBoolProperty = BooleanProperty(false);
	NumberProperty mNumberProperty = NumberProperty(FHash256::From("0x10"));
	args.Add(&mAddressProperty);
	args.Add(&mBoolProperty);
	args.Add(&mNumberProperty);
	auto retval = abiEncoder.MyEncode("cupcakeBalances(address,bool,uint256)", args);
	UE_LOG(LogTemp, Display, TEXT("Encoder message is: %s"), *retval);

	auto fixedByteCoder = FixedByteCoder();
	auto x = fixedByteCoder.Encode("abc");
	
	//provider.TransactionReceipt(TransactionHash);

	*/
	
	// Make the test pass by returning true, or fail by returning false.
	return true;
}


//e18a7b9200000000000000000000000081b35475847c79ce03e9ce407d93a40475b6a16600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010
