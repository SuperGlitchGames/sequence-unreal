Sequence Unreal Plugin
======================

This plugin comes with everything needed to get connected and send transactions via web3.

Note: all files and folders talked about in this readme will be found in the following directories with in your project folder
/Plugins/SequencePlugin/Source/SequencePlugin/Private 
or
/Plugins/SequencePlugin/Source/SequencePlugin/Public

=========================================================================================================================

!!!BEFORE YOU CAN SAFELY USE THIS!!!
You must provide an encryption key implementation at SequenceEncryptor.cpp Function GetStoredKey,
This function must be implemented to provide a securely stored private key that will be used to encrypt &
decrypt client information. Failure to do so will result in NO information being stored or in the event you
do not use a securely stored key, can result in client information being stored insecurely on their systems.
!!!BEFORE YOU CAN SAFELY USE THIS!!!

=========================================================================================================================
Getting Started

1) Once you have the the SequencePlugin folder & if you are on a fresh project, you'll need to go to your project directory
and create a Plugins Folder in it, then copy over the Sequence Plugin folder into that Plugins folder.

2) Launch your project, once open allow it to update the UProject Settings.

3) To find the SequencePlugin content folder in your content drawer enable view plugin content

4) If you wish to use the in built sequence UI for login
4a) Create an actor you wish to be responsible for the SequenceUI then attach the sequence pawn to it
4b) Setup your actor similar to how it's setup in Custom_Spectator_Pawn being sure to bind to the delegate
that gives you Credentials (AuthSuccess)

5) Once you have those credentials you'll need to forward them to your own C++ backend in order to use the SequenceAPI

6) Using a USequenceWallet::Make(Credentials) or USequenceWallet::Make(Credentials,ProviderURL), you'll create a SequenceWallet
with all the API calls you require.

7) Once you've created the USequenceWallet, be sure to call Register immediately to get registered credentials.

[IF you are using your own UI you'll need to do the following]

1) In a C++ backend with a series of UFUNCTIONS setup similarly to SequenceBackendManager you'll want to create a
UAuthenticator Object, this object will manage the authentication side of Sequence.

2) Be sure to bind to the Delegates for AuthSuccess, AuthFailure, AuthRequires Code prior to making any signin calls

3) You'll initiate Auth with either SocialSignin(SigninType) OR EmailSignin(email) calls.

4) If using emailSignin be sure to have a call routed for EmailCode(codeIn) to the Authenticator in order to continue the
auth process.

5) Upon successful Auth intercept the credentials from AuthSuccess and pass them into your USequenceWallet object
using a USequenceWallet::Make(Credentials) or USequenceWallet::Make(Credentials, ProviderURL);

6) Then call Register to get your session registered and you'll be able to use all other SequenceAPI calls from this point on

=========================================================================================================================

The subset of the SequenceAPI can be found at

SequenceAPI.h

You can directly interact with the chain via Provider.h found in the ETH folder

//To setup your game with the sequence plugin//
Simply attach the sequence pawn component to an actor,

The sequence pawn component has functions to do the following:

Setup Sequence (sets up the sequence based systems)

Show GUI Shows the UI

Hide GUI Hides the UI

GUI Visible Simple Visibility test for the UI

Switch Platform (Switches which mode the UI will be in and how it will be displayed)
Modes:
Desktop (default)
Mobile Portrait (Custom built for portrait mode reducing the X width where ever possible)
Mobile Landscape

=========================================================================================================================
Using the Sequence Unreal API

The Sequence Unreal plugin is broken up into a few different 
parts in order to manage all of the differing functionality
Here we'll go through each parts purpose then summarize how to use them all together below

**************************
NOTE: we automatically store credentials on disk in the
following cases:
1) On Successful Auth
2) On Successful Registering of a session
3) On Successful Closing of a session
**************************

************** UAuthenticator **************
UAuthenticator

This UObject's purpose is to handle authenticating with Sequence Systems

Key parts
Delegates:

/*
This event is fired when the authenticator requires a
code from the user in the UI, if you're using a custom UI
collect it and send it to the UAuthenticator Object using EmailLoginCode(FString CodeIn)
*/
AuthRequiresCode

/*
This event fires out when an error occurs in the auth
process, this can be for many reasons, timeout bad settings
etc, in general you can bind to this to let you know when you
should show an error prompt if using a custom UI
*/
AuthFailure

/*
This event fires when auth is successful the event will
also include the authenticated FCredentials_BE struct
*/
AuthSuccess

//When doing a custom UI the following functions are of interest

/*
Gets the Appropriate signin URL for SSO depending on the
provider IE) Google or Apple
*/
GetSigninURL(ESocialSigninType Type)

/*
Gets the redirect URL for SSO
*/
GetRedirectURL()

/*
This is for dealing with Social based Login,
(WIP)
*/
SocialLogin(FString IDTokenIn)

/*
Used to handle email based login
*/
EmailLogin(FString EmailIn)

/*
Used to respond to the event AuthRequiresCode
include the needed Code in this function to
continue email based auth
*/
EmailLoginCode(FString CodeIn)

/*
Used to get the credentials currently stored on disk
the FStruct returned is a wrapper for FCredentials_BE,
the bool value IsValid is also included so you can check
if the credentials on disk were valid or not
*/
FStoredCredentials_BE GetStoredCredentials()

/*
Used to store the given credentials on disk
*/
StoreCredentials(FCredentials_BE CredentialsIn)

************** UAuthenticator **************

************** USequenceWallet **************

To get a USequenceWallet call either:

USequenceWallet::Make(FCredentials_BE CredentialsIn) or
USequenceWallet::Make(FCredentials_BE CredentialsIn, FString ProviderURL) where the providerURL is the url of the provider you wish to use

SequenceAPI calls:

[Note]
We make use of TFunctions with some callbacks here I'll list some example syntax for them

TSuccessCallback
const TFunction<void(FString)> OnResponse = [Capturable variables](const FString& Response)
{
//callback body where we can process Response
};

FFailureCallback
const TFunction<void(FSequenceError)> OnFailureTest = [Capturable variables](const FSequenceError& Error)
{
//callback body where we can process Error
};

One thing to be aware of is keep an eye on capturables if you have lots of nested TFunctions it's very easy to miss
something and start over writing memory. If you require lots of nesting swapping to a more unreal esque approach using
UFUNCTION callbacks helps to avoid these problems similar to how things are done in the UAuthenticator.cpp

[Note]

/*
Used to sign the given string with sequence
TSuccessCallback<FSignedMessage> OnSuccess the TFunction that is used as a callback on successful signing the provided
struct contains the desired response

FFailureCallback Callback that is sent when something goes wrong during the signing process contains an error message
*/
SignMessage(const FString& Message, const TSuccessCallback<FSignedMessage>& OnSuccess, const FFailureCallback& OnFailure);

/*
Used to send a transaction through Sequence
TArray<TUnion<...>> Transactions , a list of transactions that are either Raw, ERC20, ERC721, ERC1155,

TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess called when the transaction is successful returns the Raw json object
due to the variety of responses we can receive from this call.

FFailureCallback Callback that is sent when something goes wrong during the SendTransaction process contains an error message
*/
SendTransaction(TArray<TUnion<FRawTransaction, FERC20Transaction, FERC721Transaction, FERC1155Transaction>> Transactions, TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess, FFailureCallback OnFailure);

/*
Used to Register a session after login
*NOTE* : this call must be done before any others in order to get valid registered credentials

OnSuccess<FString> Calls back on successful Registering with sequence
OnFailure Calls back on unsuccessful registering with sequence
*/
RegisterSession(const TSuccessCallback<FString>& OnSuccess, const FFailureCallback& OnFailure);

/*
Used to list the sessions on sequence

OnSuccess Calls back on success with a list of sessions
OnFailure calls when something went wrong
*/
ListSessions(const TSuccessCallback<TArray<FSession>>& OnSuccess, const FFailureCallback& OnFailure);

/*
Used to close the current session with sequence

OnSuccess calls back on successful close
OnFailure calls when something went wrong
*/
CloseSession(const TSuccessCallback<FString>& OnSuccess, const FFailureCallback& OnFailure);

//Indexer Calls//

/*
Used to get a ping back from the sequence app
@param ChainId, the chainId you wish to use

OnSuccess returns a bool indicating ping state
OnFailure error in pinging indexer
*/
void Ping(int64 chainID, TSuccessCallback<bool> OnSuccess, FFailureCallback OnFailure);

/*
Used to get version data back from the sequence app
@param ChainId, the chainId you wish to use

OnSuccess returns version data
OnFailure error getting version data from indexer
*/
void Version(int64 chainID, TSuccessCallback<FVersion> OnSuccess, FFailureCallback OnFailure);

/*
Used to get the runtime status of the sequence app
@param ChainId, the chainId you wish to use

OnSuccess returns runtime data
OnFailure error getting runtime status
*/
void RunTimeStatus(int64 chainID, TSuccessCallback<FRuntimeStatus> OnSuccess, FFailureCallback OnFailure);

/*
Used to get the chainID from the sequence app
@param ChainId, the chainId you wish to use

OnSuccess returns the chainId
OnFailure error getting chainId
*/
void GetChainID(int64 chainID, TSuccessCallback<int64> OnSuccess, FFailureCallback OnFailure);

/*
Used to get the Ether balance from the sequence app
@param ChainId, the chainId you wish to use
@param AccountAddr the accountAddress we want to get the balance for
@return the Balance

OnSuccess returns the Ether Balance
OnFailure error getting ether balance
*/
void GetEtherBalance(int64 chainID, FString accountAddr, TSuccessCallback<FEtherBalance> OnSuccess, FFailureCallback OnFailure);

/*
Gets the token balances from the sequence app
@param ChainId, the chainId you wish to use
@param FGetTokenBalancesArgs eg)
FGetTokenBalancesArgs args;
args.accountAddress = "0x0E0f9d1c4BeF9f0B8a2D9D4c09529F260C7758A2";
args.includeMetaData = true;

OnSuccess TokenBalances
OnFailure error getting tokenBalances
*/
void GetTokenBalances(int64 chainID, FGetTokenBalancesArgs args, TSuccessCallback<FGetTokenBalancesReturn> OnSuccess, FFailureCallback OnFailure);

/*
gets the token supplies from the sequence app

@param ChainId you wish to use
@param FGetTokenSuppliesArgs eg)
FGetTokenSuppliesArgs args;
args.contractAddress = testingContractAddress;
args.includeMetaData = true;

OnSuccess TokenSupplies
OnFailure error getting token supplies
*/
void GetTokenSupplies(int64 chainID, FGetTokenSuppliesArgs args, TSuccessCallback<FGetTokenSuppliesReturn> OnSuccess, FFailureCallback OnFailure);

/*
gets the token supplies map from the sequence app

@param ChainId, the chainId you wish to use
@param FGetTokenSuppliesMapArgs, eg)
FGetTokenSuppliesMapArgs args;
TMap<FString, FTokenList> tokenMap;
TPair<FString,FTokenList> item;
tokenMap.Add(item);
args.includeMetaData = true;
args.tokenMap = tokenMap;

OnSuccess TokenSuppliesMap
OnFailure Error getting token supplies map
*/
void GetTokenSuppliesMap(int64 chainID, FGetTokenSuppliesMapArgs args, TSuccessCallback<FGetTokenSuppliesMapReturn> OnSuccess, FFailureCallback OnFailure);

/*
Get the balance updates from the sequence app

@param ChainId, the chainId you wish to use
@param FGetBalanceUpdatesArgs the balance update arg struct sent to the indexer eg)
FGetBalanceUpdatesArgs args;
args.contractAddress = "0x0E0f9d1c4BeF9f0B8a2D9D4c09529F260C7758A2";
args.page.page = 10;
args.page.more = true;

OnSuccess BalanceUpdates
OnFailure error getting balance updates
*/
void GetBalanceUpdates(int64 chainID, FGetBalanceUpdatesArgs args, TSuccessCallback<FGetBalanceUpdatesReturn> OnSuccess, FFailureCallback OnFailure);

/*
get transaction history from the sequence app

@Param ChainId the chainId you want to use
@Param FGetTransactionHistoryArgs, the transaction history argument struct required to send to the indexer
eg)
FGetTransactionHistoryArgs args;
args.filter.accountAddress = "0x0E0f9d1c4BeF9f0B8a2D9D4c09529F260C7758A2";
args.includeMetaData = true;
args.page.page = 0;
args.page.more = true;

OnSuccess TransactionHistory
OnFailure error getting transaction History
*/
void GetTransactionHistory(int64 chainID, FGetTransactionHistoryArgs args, TSuccessCallback<FGetTransactionHistoryReturn> OnSuccess, FFailureCallback OnFailure);

//Provider Calls (Note to use this properly you need to specify a provider URL when making the USequenceWallet)

//TODO need Jan to help me go through these calls and what they require//

void BlockByNumber(uint64 Number, TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess, FFailureCallback OnFailure);
void BlockByNumber(EBlockTag Tag, TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess, FFailureCallback OnFailure);
void BlockByHash(FHash256 Hash, TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess, FFailureCallback OnFailure);
void BlockNumber(TSuccessCallback<uint64> OnSuccess, FFailureCallback OnFailure);

void HeaderByNumber(uint64 Id, TSuccessCallback<FHeader> OnSuccess, FFailureCallback OnFailure);
void HeaderByNumber(EBlockTag Tag, TSuccessCallback<FHeader> OnSuccess, FFailureCallback OnFailure);
void HeaderByHash(FHash256 Hash, TSuccessCallback<FHeader> OnSuccess, FFailureCallback OnFailure);

void TransactionByHash(FHash256 Hash, TSuccessCallback<TSharedPtr<FJsonObject>> OnSuccess, FFailureCallback OnFailure);
void TransactionCount(FAddress Addr, uint64 Number, TSuccessCallback<uint64> OnSuccess, FFailureCallback OnFailure);
void TransactionCount(FAddress Addr, EBlockTag Tag, TSuccessCallback<uint64> OnSuccess, FFailureCallback OnFailure);
void TransactionReceipt(FHash256 Hash, TSuccessCallback<FTransactionReceipt> OnSuccess, FFailureCallback OnFailure);

void GetGasPrice(TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure);
void EstimateContractCallGas(FContractCall ContractCall, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure);
void EstimateDeploymentGas(FAddress From, FString Bytecode, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure);

void DeployContract(FString Bytecode, FPrivateKey PrivKey, int64 ChainId, TSuccessCallback<FAddress> OnSuccess, FFailureCallback OnFailure);
void DeployContractWithHash(FString Bytecode, FPrivateKey PrivKey, int64 ChainId, TSuccessCallbackTuple<FAddress, FUnsizedData> OnSuccess, FFailureCallback OnFailure);

void NonceAt(uint64 Number, TSuccessCallback<FBlockNonce> OnSuccess, FFailureCallback OnFailure);
void NonceAt(EBlockTag Tag, TSuccessCallback<FBlockNonce> OnSuccess, FFailureCallback OnFailure);
void SendRawTransaction(FString Data, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure);

void ChainId(TSuccessCallback<uint64> OnSuccess, FFailureCallback OnFailure);

void Call(FContractCall ContractCall, uint64 Number, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure);
void Call(FContractCall ContractCall, EBlockTag Number, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure);
void NonViewCall(FEthTransaction transaction, FPrivateKey PrivateKey, int ChainID, TSuccessCallback<FUnsizedData> OnSuccess, FFailureCallback OnFailure);
************** USequenceAPI **************

************** Packaging **************
To set your system up for Packaging please refer to the following links
For Windows, Mac
https://docs.unrealengine.com/5.0/en-US/packaging-unreal-engine-projects/#:~:text=Clicking%20File%20%3E%20Package%20Project%20%3E%20Packaging,options%20for%20the%20packaging%20feature.&text=The%20build%20configuration%20to%20compile,a%20code%20project%2C%20select%20DebugGame.
For Android
https://docs.unrealengine.com/4.27/en-US/SharingAndReleasing/Mobile/Android/PackagingAndroidProject/
For IOS
https://docs.unrealengine.com/5.0/en-US/packaging-ios-projects-in-unreal-engine/

For IOS you also need to setup provisioning refer to these docs to achieve that:

https://docs.unrealengine.com/5.1/en-US/setting-up-ios-tvos-and-ipados-provisioning-profiles-and-signing-certificates-for-unreal-engine-projects/
************** Packaging **************