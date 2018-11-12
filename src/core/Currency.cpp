#include "Currency.h"
#include <cctype>
#include <boost/algorithm/string/trim.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/lexical_cast.hpp>
#include "common/Base58.h"
#include "int-util.h"
#include "common/StringTools.h"

#include "Account.h"
#include "base/CryptoNoteBasicImpl.h"
#include "base/CryptoNoteFormatUtils.h"
#include "base/CryptoNoteTools.h"
#include "trans/TransactionExtra.h"
#include "core/UpgradeDetector.h"

#undef ERROR

using namespace Logging;
using namespace Common;

namespace CryptoNote {

const std::vector<uint64_t> Currency::PRETTY_AMOUNTS = {
  1, 2, 3, 4, 5, 6, 7, 8, 9,
  10, 20, 30, 40, 50, 60, 70, 80, 90,
  100, 200, 300, 400, 500, 600, 700, 800, 900,
  1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
  10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000,
  100000, 200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000,
  1000000, 2000000, 3000000, 4000000, 5000000, 6000000, 7000000, 8000000, 9000000,
  10000000, 20000000, 30000000, 40000000, 50000000, 60000000, 70000000, 80000000, 90000000,
  100000000, 200000000, 300000000, 400000000, 500000000, 600000000, 700000000, 800000000, 900000000,
  1000000000, 2000000000, 3000000000, 4000000000, 5000000000, 6000000000, 7000000000, 8000000000, 9000000000,
  10000000000, 20000000000, 30000000000, 40000000000, 50000000000, 60000000000, 70000000000, 80000000000, 90000000000,
  100000000000, 200000000000, 300000000000, 400000000000, 500000000000, 600000000000, 700000000000, 800000000000, 900000000000,
  1000000000000, 2000000000000, 3000000000000, 4000000000000, 5000000000000, 6000000000000, 7000000000000, 8000000000000, 9000000000000,
  10000000000000, 20000000000000, 30000000000000, 40000000000000, 50000000000000, 60000000000000, 70000000000000, 80000000000000, 90000000000000,
  100000000000000, 200000000000000, 300000000000000, 400000000000000, 500000000000000, 600000000000000, 700000000000000, 800000000000000, 900000000000000,
  1000000000000000, 2000000000000000, 3000000000000000, 4000000000000000, 5000000000000000, 6000000000000000, 7000000000000000, 8000000000000000, 9000000000000000,
  10000000000000000, 20000000000000000, 30000000000000000, 40000000000000000, 50000000000000000, 60000000000000000, 70000000000000000, 80000000000000000, 90000000000000000,
  100000000000000000, 200000000000000000, 300000000000000000, 400000000000000000, 500000000000000000, 600000000000000000, 700000000000000000, 800000000000000000, 900000000000000000,
  1000000000000000000, 2000000000000000000, 3000000000000000000, 4000000000000000000, 5000000000000000000, 6000000000000000000, 7000000000000000000, 8000000000000000000, 9000000000000000000,
  10000000000000000000ull
};
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::init() {
  if (!generateGenesisBlock()) {
    logger(ERROR, BRIGHT_RED) << "Failed to generate genesis block";
    return false;
  }

  if (!get_block_hash(m_genesisBlock, m_genesisBlockHash)) {
    logger(ERROR, BRIGHT_RED) << "Failed to get genesis block hash";
    return false;
  }

  if (isTestnet()) {
    m_upgradeHeightv0 = static_cast<uint32_t>(-1);
    m_upgradeHeightv1 = static_cast<uint32_t>(-1);
    m_upgradeHeightv2 = static_cast<uint32_t>(-1);
    m_upgradeHeightv3 = static_cast<uint32_t>(-1);
    m_blocksFileName = "testnet_" + m_blocksFileName;
    m_blocksCacheFileName = "testnet_" + m_blocksCacheFileName;
    m_blockIndexesFileName = "testnet_" + m_blockIndexesFileName;
    m_txPoolFileName = "testnet_" + m_txPoolFileName;
    m_blockchainIndicesFileName = "testnet_" + m_blockchainIndicesFileName;
  }

  return true;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::generateGenesisBlock() {
  m_genesisBlock = boost::value_initialized<Block>();

  // Hard code coinbase tx in genesis block, because "tru" generating tx use random, but genesis should be always the same
  std::string genesisCoinbaseTxHex = GENESIS_COINBASE_TX_HEX;
  BinaryArray minerTxBlob;

  bool r =
    fromHex(genesisCoinbaseTxHex, minerTxBlob) &&
    fromBinaryArray(m_genesisBlock.baseTransaction, minerTxBlob);

  if (!r) {
    logger(ERROR, BRIGHT_RED) << "failed to parse coinbase tx from hard coded blob";
    return false;
  }

  m_genesisBlock.majorVersion = CURRENT_BLOCK_MAJOR;
  m_genesisBlock.minorVersion = BLOCK_MINOR_VERSION_0;
  m_genesisBlock.timestamp = GENESIS_TIMESTAMP;
  m_genesisBlock.nonce = GENESIS_NONCE;

  if (m_testnet) {
    ++m_genesisBlock.nonce;
  }

  //miner::find_nonce_for_given_block(bl, 1, 0);
  return true;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
uint32_t Currency::upgradeHeight(uint8_t majorVersion) const {
  if (majorVersion == (CURRENT_BLOCK_MAJOR + 1)) {
    return m_upgradeHeightv0;
  } else if (majorVersion == (CURRENT_BLOCK_MAJOR + 2)) {
    return m_upgradeHeightv1;
  } else if (majorVersion == (CURRENT_BLOCK_MAJOR + 3)) {
    return m_upgradeHeightv2;
  } else if (majorVersion == (NEXT_BLOCK_MAJOR_LIMIT)) {
    return m_upgradeHeightv3;
  } else {
    return static_cast<uint32_t>(-1);
  }
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
size_t Currency::blockGrantedFullRewardZoneByBlockVersion(uint8_t blockMajorVersion) const {
  if (blockMajorVersion >= (CURRENT_BLOCK_MAJOR + 2)) {
    return m_blockGrantedFullRewardZone;
  } else {
    return CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_v0;
  }
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::getBlockReward(uint8_t blockMajorVersion, size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins,
  uint64_t fee, uint32_t height, uint64_t& reward, int64_t& emissionChange) const {

  assert(m_emissionSpeedFactor > 0 && m_emissionSpeedFactor <= 8 * sizeof(uint64_t));
  uint64_t baseReward = (m_moneySupply - alreadyGeneratedCoins) >> m_emissionSpeedFactor;
   if (height == 1) {
      baseReward = m_genesisBlockReward;
      //std::cout << "Genesis block reward: " << baseReward << std::endl;
   }
   // Tail emission
   if ((height >= 2) && (height <= 300000)){
      uint64_t bad_tail_emission_reward = uint64_t(5 * COIN);
   if (alreadyGeneratedCoins + bad_tail_emission_reward <= m_moneySupply || baseReward < bad_tail_emission_reward)
   {
      baseReward = bad_tail_emission_reward;
     // std::cout << "Found block reward: " << baseReward << std::endl;
   }
   } 
      
   size_t blockGrantedFullRewardZone = blockGrantedFullRewardZoneByBlockVersion(blockMajorVersion);
   medianSize = std::max(medianSize, blockGrantedFullRewardZone);
   if (currentBlockSize > UINT64_C(2) * medianSize) {
      logger(TRACE) << "Block cumulative size is too big: " << currentBlockSize << ", expected less than " << 2 * medianSize;
      return false;
   }

      uint64_t penalizedBaseReward = getPenalizedAmount(baseReward, medianSize, currentBlockSize);
      uint64_t penalizedFee = blockMajorVersion >= (CURRENT_BLOCK_MAJOR + 1) ? getPenalizedAmount(fee, medianSize, currentBlockSize) : fee;
   if (cryptonoteCoinVersion() == 1) {
      emissionChange = penalizedBaseReward;
      reward = penalizedBaseReward + fee;
   } else { 
      penalizedFee = getPenalizedAmount(fee, medianSize, currentBlockSize); 
      emissionChange = penalizedBaseReward - (fee - penalizedFee);
      reward = penalizedBaseReward + penalizedFee;
   }

   return true;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
uint64_t Currency::calculateInterest(uint64_t amount, uint32_t term, uint32_t height) const {
  assert(m_depositMinTerm <= term && term <= m_depositMaxTerm);
  assert(static_cast<uint64_t>(term)* m_depositMaxTotalRate > m_depositMinTotalRateFactor);

  uint64_t a = static_cast<uint64_t>(term) * m_depositMaxTotalRate - m_depositMinTotalRateFactor;
  uint64_t bHi;
  uint64_t bLo = mul128(amount, a, &bHi);

  uint64_t cHi;
  uint64_t cLo;
  assert(std::numeric_limits<uint32_t>::max() / 100 > m_depositMaxTerm);
  div128_32(bHi, bLo, static_cast<uint32_t>(100 * m_depositMaxTerm), &cHi, &cLo);
  assert(cHi == 0);

  // early depositor multiplier
  uint64_t interestHi;
  uint64_t interestLo;
  if (height >= m_edMultiFac){
      interestLo = mul128(cLo, m_multiFac, &interestHi);
      assert(interestHi == 0);
  } else {
      interestHi = cHi;
      interestLo = cLo;
  }

  return interestLo;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
uint64_t Currency::calculateTotalTransactionInterest(const Transaction& tx, uint32_t height) const {
  uint64_t interest = 0;
  for (const TransactionInput& input : tx.inputs) {
    if (input.type() == typeid(MultisignatureInput)) {
      const MultisignatureInput& multisignatureInput = boost::get<MultisignatureInput>(input);
      if (multisignatureInput.term != 0) {
        interest += calculateInterest(multisignatureInput.amount, multisignatureInput.term, height);
      }
    }
  }

  return interest;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
uint64_t Currency::getTransactionInputAmount(const TransactionInput& in, uint32_t height) const {
  if (in.type() == typeid(KeyInput)) {
    return boost::get<KeyInput>(in).amount;
  } else if (in.type() == typeid(MultisignatureInput)) {
    const MultisignatureInput& multisignatureInput = boost::get<MultisignatureInput>(in);
    if (multisignatureInput.term == 0) {
      return multisignatureInput.amount;
    } else {
      return multisignatureInput.amount + calculateInterest(multisignatureInput.amount, multisignatureInput.term, height);
    }
  } else if (in.type() == typeid(BaseInput)) {
    return 0;
  } else {
    assert(false);
    return 0;
  }
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
uint64_t Currency::getTransactionAllInputsAmount(const Transaction& tx, uint32_t height) const {
  uint64_t amount = 0;
  for (const auto& in : tx.inputs) {
    amount += getTransactionInputAmount(in, height);
  }

  return amount;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::getTransactionFee(const Transaction& tx, uint64_t& fee, uint32_t height) const {
  uint64_t amount_in = 0;
  uint64_t amount_out = 0;

  //if (tx.inputs.size() == 0)// || tx.outputs.size() == 0) //0 outputs needed in TestGenerator::constructBlock
  //	  return false;

  for (const auto& in : tx.inputs) {
    amount_in += getTransactionInputAmount(in, height);
  }

  for (const auto& o : tx.outputs) {
    amount_out += o.amount;
  }

  if (amount_out > amount_in){
    // interest shows up in the output of the W/D transactions and W/Ds always have min fee
    if (tx.inputs.size() > 0 && tx.outputs.size() > 0 && amount_out > amount_in + m_minimumFee) {
      fee = m_minimumFee;
    } else {
      return false;
    }
  } else {
	   fee = amount_in - amount_out;
  }

  return true;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
uint64_t Currency::getTransactionFee(const Transaction& tx, uint32_t height) const {
  uint64_t r = 0;
  if (!getTransactionFee(tx, r, height)) {
    r = 0;
  }

  return r;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
size_t Currency::maxBlockCumulativeSize(uint64_t height) const {
  assert(height <= std::numeric_limits<uint64_t>::max() / m_maxBlockSizeGrowthSpeedNumerator);
  size_t maxSize = static_cast<size_t>(m_maxBlockSizeInitial +
    (height * m_maxBlockSizeGrowthSpeedNumerator) / m_maxBlockSizeGrowthSpeedDenominator);

  assert(maxSize >= m_maxBlockSizeInitial);
  return maxSize;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::constructMinerTx(uint8_t blockMajorVersion, uint32_t height, size_t medianSize, uint64_t alreadyGeneratedCoins, size_t currentBlockSize,
  uint64_t fee, const AccountPublicAddress& minerAddress, Transaction& tx,
  const BinaryArray& extraNonce/* = BinaryArray()*/, size_t maxOuts/* = 1*/) const {
  tx.inputs.clear();
  tx.outputs.clear();
  tx.extra.clear();

  KeyPair txkey = generateKeyPair();
  addTransactionPublicKeyToExtra(tx.extra, txkey.publicKey);
  if (!extraNonce.empty()) {
    if (!addExtraNonceToTransactionExtra(tx.extra, extraNonce)) {
      return false;
    }
  }

  BaseInput in;
  in.blockIndex = height;

  uint64_t blockReward;
  int64_t emissionChange;
  if (!getBlockReward(blockMajorVersion, medianSize, currentBlockSize, alreadyGeneratedCoins, fee, height, blockReward, emissionChange)) {
    logger(INFO) << "Block is too big";
    return false;
  }

  std::vector<uint64_t> outAmounts;
  decompose_amount_into_digits(blockReward, m_defaultDustThreshold,
    [&outAmounts](uint64_t a_chunk) { outAmounts.push_back(a_chunk); },
    [&outAmounts](uint64_t a_dust) { outAmounts.push_back(a_dust); });

  if (!(1 <= maxOuts)) {
    logger(ERROR, BRIGHT_RED) << "max_out must be non-zero";
    return false;
  }

  while (maxOuts < outAmounts.size()) {
    outAmounts[outAmounts.size() - 2] += outAmounts.back();
    outAmounts.resize(outAmounts.size() - 1);
  }

  uint64_t summaryAmounts = 0;
  for (size_t no = 0; no < outAmounts.size(); no++) {
    Crypto::KeyDerivation derivation = boost::value_initialized<Crypto::KeyDerivation>();
    Crypto::PublicKey outEphemeralPubKey = boost::value_initialized<Crypto::PublicKey>();

    bool r = Crypto::generate_key_derivation(minerAddress.viewPublicKey, txkey.secretKey, derivation);

    if (!(r)) {
      logger(ERROR, BRIGHT_RED)
        << "while creating outs: failed to generate_key_derivation("
        << minerAddress.viewPublicKey << ", " << txkey.secretKey << ")";

      return false;
    }

    r = Crypto::derive_public_key(derivation, no, minerAddress.spendPublicKey, outEphemeralPubKey);

    if (!(r)) {
      logger(ERROR, BRIGHT_RED)
        << "while creating outs: failed to derive_public_key("
        << derivation << ", " << no << ", "
        << minerAddress.spendPublicKey << ")";

      return false;
    }

    KeyOutput tk;
    tk.key = outEphemeralPubKey;

    TransactionOutput out;
    summaryAmounts += out.amount = outAmounts[no];
    out.target = tk;
    tx.outputs.push_back(out);
  }

  if (!(summaryAmounts == blockReward)) {
    logger(ERROR, BRIGHT_RED) << "Failed to construct miner tx, summaryAmounts = " << summaryAmounts << " not equal blockReward = " << blockReward;
    return false;
  }

  tx.version = CURRENT_TRANSACTION_VERSION;
  // lock
  tx.unlockTime = height + m_minedMoneyUnlockWindow;
  tx.inputs.push_back(in);
  return true;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::isFusionTransaction(const std::vector<uint64_t>& inputsAmounts, const std::vector<uint64_t>& outputsAmounts, size_t size) const {
  if (size > fusionTxMaxSize()) {
    return false;
  }

  if (inputsAmounts.size() < fusionTxMinInputCount()) {
    return false;
  }

  if (inputsAmounts.size() < outputsAmounts.size() * fusionTxMinInOutCountRatio()) {
    return false;
  }

  uint64_t inputAmount = 0;
  for (auto amount: inputsAmounts) {
    if (amount < defaultDustThreshold()) {
      return false;
    }

    inputAmount += amount;
  }

  std::vector<uint64_t> expectedOutputsAmounts;
  expectedOutputsAmounts.reserve(outputsAmounts.size());
  decomposeAmount(inputAmount, defaultDustThreshold(), expectedOutputsAmounts);
  std::sort(expectedOutputsAmounts.begin(), expectedOutputsAmounts.end());

  return expectedOutputsAmounts == outputsAmounts;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::isFusionTransaction(const Transaction& transaction, size_t size) const {
  assert(getObjectBinarySize(transaction) == size);

  std::vector<uint64_t> outputsAmounts;
  outputsAmounts.reserve(transaction.outputs.size());
  for (const TransactionOutput& output : transaction.outputs) {
    outputsAmounts.push_back(output.amount);
  }

  return isFusionTransaction(getInputsAmounts(transaction), outputsAmounts, size);
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::isFusionTransaction(const Transaction& transaction) const {
  return isFusionTransaction(transaction, getObjectBinarySize(transaction));
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::isAmountApplicableInFusionTransactionInput(uint64_t amount, uint64_t threshold) const {
  uint8_t ignore;
  return isAmountApplicableInFusionTransactionInput(amount, threshold, ignore);
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::isAmountApplicableInFusionTransactionInput(uint64_t amount, uint64_t threshold, uint8_t& amountPowerOfTen) const {
  if (amount >= threshold) {
    return false;
  }

  if (amount < defaultDustThreshold()) {
    return false;
  }

  auto it = std::lower_bound(PRETTY_AMOUNTS.begin(), PRETTY_AMOUNTS.end(), amount);
  if (it == PRETTY_AMOUNTS.end() || amount != *it) {
    return false;
  }

  amountPowerOfTen = static_cast<uint8_t>(std::distance(PRETTY_AMOUNTS.begin(), it) / 9);
  return true;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
std::string Currency::accountAddressAsString(const AccountBase& account) const {
  return getAccountAddressAsStr(m_publicAddressBase58Prefix, account.getAccountKeys().address);
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
std::string Currency::accountAddressAsString(const AccountPublicAddress& accountPublicAddress) const {
  return getAccountAddressAsStr(m_publicAddressBase58Prefix, accountPublicAddress);
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::parseAccountAddressString(const std::string& str, AccountPublicAddress& addr) const {
  uint64_t prefix;
  if (!CryptoNote::parseAccountAddressString(prefix, addr, str)) {
    return false;
  }

  if (prefix != m_publicAddressBase58Prefix) {
    logger(DEBUGGING) << "Wrong address prefix: " << prefix << ", expected " << m_publicAddressBase58Prefix;
    return false;
  }

  return true;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
std::string Currency::formatAmount(uint64_t amount) const {
  std::string s = std::to_string(amount);
  if (s.size() < m_numberOfDecimalPlaces + 1) {
    s.insert(0, m_numberOfDecimalPlaces + 1 - s.size(), '0');
  }

  s.insert(s.size() - m_numberOfDecimalPlaces, ".");
  return s;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
std::string Currency::formatAmount(int64_t amount) const {
  std::string s = formatAmount(static_cast<uint64_t>(std::abs(amount)));

  if (amount < 0) {
    s.insert(0, "-");
  }

  return s;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::parseAmount(const std::string& str, uint64_t& amount) const {
  std::string strAmount = str;
  boost::algorithm::trim(strAmount);

  size_t pointIndex = strAmount.find_first_of('.');
  size_t fractionSize;

  if (std::string::npos != pointIndex) {
    fractionSize = strAmount.size() - pointIndex - 1;
    while (m_numberOfDecimalPlaces < fractionSize && '0' == strAmount.back()) {
      strAmount.erase(strAmount.size() - 1, 1);
      --fractionSize;
    }

    if (m_numberOfDecimalPlaces < fractionSize) {
      return false;
    }

    strAmount.erase(pointIndex, 1);
  } else {
    fractionSize = 0;
  }

  if (strAmount.empty()) {
    return false;
  }

  if (!std::all_of(strAmount.begin(), strAmount.end(), ::isdigit)) {
    return false;
  }

  if (fractionSize < m_numberOfDecimalPlaces) {
    strAmount.append(m_numberOfDecimalPlaces - fractionSize, '0');
  }

  return Common::fromString(strAmount, amount);
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------/
// Legacy difficulty algorithm
difficulty_type Currency::nextDifficulty1(std::vector<uint64_t> timestamps,
  std::vector<difficulty_type> cumulativeDifficulties) const {
  assert(m_difficultyWindow >= 2);

  if (timestamps.size() > m_difficultyWindow) {
    timestamps.resize(m_difficultyWindow);
    cumulativeDifficulties.resize(m_difficultyWindow);
  }

  size_t length = timestamps.size();
  assert(length == cumulativeDifficulties.size());
  assert(length <= m_difficultyWindow);
  if (length <= 1) {
    return 1;
  }

  sort(timestamps.begin(), timestamps.end());

  size_t cutBegin, cutEnd;
  assert(2 * m_difficultyCut <= m_difficultyWindow - 2);
  if (length <= m_difficultyWindow - 2 * m_difficultyCut) {
    cutBegin = 0;
    cutEnd = length;
  } else {
    cutBegin = (length - (m_difficultyWindow - 2 * m_difficultyCut) + 1) / 2;
    cutEnd = cutBegin + (m_difficultyWindow - 2 * m_difficultyCut);
  }
  assert(/*cut_begin >= 0 &&*/ cutBegin + 2 <= cutEnd && cutEnd <= length);
  uint64_t timeSpan = timestamps[cutEnd - 1] - timestamps[cutBegin];
  if (timeSpan == 0) {
    timeSpan = 1;
  }

  difficulty_type totalWork = cumulativeDifficulties[cutEnd - 1] - cumulativeDifficulties[cutBegin];
  assert(totalWork > 0);

  uint64_t low, high;
  low = mul128(totalWork, m_difficultyTarget, &high);
  if (high != 0 || low + timeSpan - 1 < low) {
    return 0;
  }

  return (low + timeSpan - 1) / timeSpan;
}

// Zawy's LMWA difficulty algo
difficulty_type Currency::nextDifficulty(std::vector<uint64_t> timestamps,
	std::vector<difficulty_type> cumulativeDifficulties, uint64_t height) const {

	int64_t T = m_difficultyTarget; // set to 20 temporarily
        int64_t N = m_difficultyWindowv1 - 1; //  N=45, 60, and 90 for T=600, 120, 60.
        int64_t FTL = m_blockFutureTimeLimit; // < 3xT
	int64_t L(0), ST, sum_3_ST(0), next_D, prev_D;

	// Hardcode difficulty for 61 blocks after fork height: 
	//if (height >= parameters::UPGRADE_HEIGHT_V4 && height <= parameters::UPGRADE_HEIGHT_V4 + N) {
		//return 1000000000; //by default put this for not overloaded diff ~ Yuka
	//}

	// TS and CD vectors must be size N+1 after startup, and element N is most recent block.

	// If coin is starting, this will be activated.
	int64_t initial_difficulty_guess = 1; // Dev needs to select this. Guess low.
	if (timestamps.size() <= static_cast<uint64_t>(N)) {
		return initial_difficulty_guess;
	}

	// N is most recently solved block. i must be signed
	for (int64_t i = 1; i <= N; i++) {
		// +/- FTL limits are bad timestamp protection.  6xT limits drop in D to reduce oscillations.
		ST = std::max(-FTL, std::min((int64_t)(timestamps[i]) - (int64_t)(timestamps[i - 1]), 6 * T));
		L += ST * i; // Give more weight to most recent blocks.
					 // Do these inside loop to capture -FTL and +6*T limitations.
		if (i > N - 3) { sum_3_ST += ST; }
	}

	// Calculate next_D = avgD * T / LWMA(STs) using integer math
	next_D = ((cumulativeDifficulties[N] - cumulativeDifficulties[0])*T*(N + 1) * 99) / (100 * 2 * L);

	// Implement LWMA-2 changes from LWMA
	prev_D = cumulativeDifficulties[N] - cumulativeDifficulties[N - 1];
	if (sum_3_ST < (8 * T) / 10) { next_D = (prev_D * 110) / 100; }

	return static_cast<uint64_t>(next_D); //turn to anata diff LWMA ~ Yuka

	// next_Target = sumTargets*L*2/0.998/T/(N+1)/N/N; // To show the difference.
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
bool Currency::checkProofOfWorkV1(Crypto::cn_context& context, const Block& block, difficulty_type currentDiffic,
		Crypto::Hash& proofOfWork) const {
		if (CURRENT_BLOCK_MAJOR != block.majorVersion) {
			return false;
		}

		if (!get_block_longhash(context, block, proofOfWork)) {
			return false;
		}

		return check_hash(proofOfWork, currentDiffic);
	}

	bool Currency::checkProofOfWorkV2(Crypto::cn_context& context, const Block& block, difficulty_type currentDiffic,
		Crypto::Hash& proofOfWork) const {
		if (block.majorVersion < (CURRENT_BLOCK_MAJOR + 1)) {
			return false;
		}

		if (!get_block_longhash(context, block, proofOfWork)) {
			return false;
		}

		if (!check_hash(proofOfWork, currentDiffic)) {
			return false;
		}

		TransactionExtraMergeMiningTag mmTag;
		if (!getMergeMiningTagFromExtra(block.parentBlock.baseTransaction.extra, mmTag)) {
			logger(ERROR) << "merge mining tag wasn't found in extra of the parent block miner transaction";
			return false;
		}

		if (8 * sizeof(m_genesisBlockHash) < block.parentBlock.blockchainBranch.size()) {
			return false;
		}

		Crypto::Hash auxBlockHeaderHash;
		if (!get_aux_block_header_hash(block, auxBlockHeaderHash)) {
			return false;
		}

		Crypto::Hash auxBlocksMerkleRoot;
		Crypto::tree_hash_from_branch(block.parentBlock.blockchainBranch.data(), block.parentBlock.blockchainBranch.size(),
			auxBlockHeaderHash, &m_genesisBlockHash, auxBlocksMerkleRoot);

		if (auxBlocksMerkleRoot != mmTag.merkleRoot) {
			logger(ERROR, BRIGHT_YELLOW) << "Aux block hash wasn't found in merkle tree";
			return false;
		}

		return true;
	}

	bool Currency::checkProofOfWork(Crypto::cn_context& context, const Block& block, difficulty_type currentDiffic, Crypto::Hash& proofOfWork) const {
		switch (block.majorVersion) {
		case CURRENT_BLOCK_MAJOR:
			return checkProofOfWorkV1(context, block, currentDiffic, proofOfWork);

		case CURRENT_BLOCK_MAJOR + 1:
		case CURRENT_BLOCK_MAJOR + 2:
		case CURRENT_BLOCK_MAJOR + 3:
                case NEXT_BLOCK_MAJOR_LIMIT:
			return checkProofOfWorkV2(context, block, currentDiffic, proofOfWork);
		}

		logger(ERROR, BRIGHT_RED) << "Unknown block major version: " << block.majorVersion << "." << block.minorVersion;
		return false;
	}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
size_t Currency::getApproximateMaximumInputCount(size_t transactionSize, size_t outputCount, size_t mixinCount) const {
  const size_t KEY_IMAGE_SIZE = sizeof(Crypto::KeyImage);
  const size_t OUTPUT_KEY_SIZE = sizeof(decltype(KeyOutput::key));
  const size_t AMOUNT_SIZE = sizeof(uint64_t) + 2; // varint
  const size_t GLOBAL_INDEXES_VECTOR_SIZE_SIZE = sizeof(uint8_t); // varint
  const size_t GLOBAL_INDEXES_INITIAL_VALUE_SIZE = sizeof(uint32_t); // varint
  const size_t GLOBAL_INDEXES_DIFFERENCE_SIZE = sizeof(uint32_t); // varint
  const size_t SIGNATURE_SIZE = sizeof(Crypto::Signature);
  const size_t EXTRA_TAG_SIZE = sizeof(uint8_t);
  const size_t INPUT_TAG_SIZE = sizeof(uint8_t);
  const size_t OUTPUT_TAG_SIZE = sizeof(uint8_t);
  const size_t PUBLIC_KEY_SIZE = sizeof(Crypto::PublicKey);
  const size_t TRANSACTION_VERSION_SIZE = sizeof(uint8_t);
  const size_t TRANSACTION_UNLOCK_TIME_SIZE = sizeof(uint64_t);

  const size_t outputsSize = outputCount * (OUTPUT_TAG_SIZE + OUTPUT_KEY_SIZE + AMOUNT_SIZE);
  const size_t headerSize = TRANSACTION_VERSION_SIZE + TRANSACTION_UNLOCK_TIME_SIZE + EXTRA_TAG_SIZE + PUBLIC_KEY_SIZE;
  const size_t inputSize = INPUT_TAG_SIZE + AMOUNT_SIZE + KEY_IMAGE_SIZE + SIGNATURE_SIZE + GLOBAL_INDEXES_VECTOR_SIZE_SIZE +
    GLOBAL_INDEXES_INITIAL_VALUE_SIZE + mixinCount * (GLOBAL_INDEXES_DIFFERENCE_SIZE + SIGNATURE_SIZE);

  return (transactionSize - headerSize - outputsSize) / inputSize;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
CurrencyBuilder::CurrencyBuilder(Logging::ILogger& log) : m_currency(log) {
  maxBlockNumber(CRYPTONOTE_MAX_BLOCK_NUMBER);
  maxBlockBlobSize(CRYPTONOTE_MAX_BLOCK_BLOB_SIZE);
  maxTxSize(CRYPTONOTE_MAX_TX_SIZE);
  publicAddressBase58Prefix(CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
  minedMoneyUnlockWindow(CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);

  timestampCheckWindow(BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW);
  blockFutureTimeLimit(CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT);

  moneySupply(MONEY_SUPPLY);
  emissionSpeedFactor(EMISSION_SPEED_FACTOR);
  cryptonoteCoinVersion(CRYPTONOTE_COIN_VERSION);
  genesisBlockReward(PRE_BLOCK_REWARD);
  tailemisionReward(TAIL_EMISSION_REWARD);

  rewardBlocksWindow(CRYPTONOTE_REWARD_BLOCKS_WINDOW);
  minMixin(MIN_MIXIN);
  blockGrantedFullRewardZone(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
  blockGrantedFullRewardZonev0(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_v0);
  blockGrantedFullRewardZonev1(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_v1);
  minerTxBlobReservedSize(CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE);

  numberOfDecimalPlaces(CRYPTONOTE_DISPLAY_DECIMAL_POINT);

  minimumFee(MINIMUM_FEE);
  defaultDustThreshold(DEFAULT_DUST_THRESHOLD);

  difficultyTarget(DIFFICULTY_TARGET);
  difficultyWindow(DIFFICULTY_WINDOW);
  difficultyWindowv0(DIFFICULTY_WINDOW_v0);
  difficultyWindowv1(DIFFICULTY_WINDOW_v1);
  difficultyCut(DIFFICULTY_CUT);

  depositMinAmount(DEPOSIT_MIN_AMOUNT);
  depositMinTerm(DEPOSIT_MIN_TERM);
  depositMaxTerm(DEPOSIT_MAX_TERM);
  depositMinTotalRateFactor(DEPOSIT_MIN_TOTAL_RATE_FACTOR);
  depositMaxTotalRate(DEPOSIT_MAX_TOTAL_RATE);

  multiFac(MULTIPLIER_FACTOR);
  edMultiFac(END_MULTIPLIER_BLOCK);

  maxBlockSizeInitial(MAX_BLOCK_SIZE_INITIAL);
  maxBlockSizeGrowthSpeedNumerator(MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR);
  maxBlockSizeGrowthSpeedDenominator(MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR);

  lockedTxAllowedDeltaSeconds(CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS);
  lockedTxAllowedDeltaBlocks(CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);

  mempoolTxLiveTime(CRYPTONOTE_MEMPOOL_TX_LIVETIME);
  mempoolTxFromAltBlockLiveTime(CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME);
  numberOfPeriodsToForgetTxDeletedFromPool(CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL);

  transactionMaxSize(CRYPTONOTE_MAX_TX_SIZE_LIMIT);
  fusionTxMaxSize(FUSION_TX_MAX_SIZE);
  fusionTxMinInputCount(FUSION_TX_MIN_INPUT_COUNT);
  fusionTxMinInOutCountRatio(FUSION_TX_MIN_IN_OUT_COUNT_RATIO);

  upgradeHeightv0(UPGRADE_HEIGHT_v0);
  upgradeHeightv1(UPGRADE_HEIGHT_v1);
  upgradeHeightv2(UPGRADE_HEIGHT_v2);
  upgradeHeightv3(UPGRADE_HEIGHT_v3);
  upgradeVotingThreshold(UPGRADE_VOTING_THRESHOLD);
  upgradeVotingWindow(UPGRADE_VOTING_WINDOW);
  upgradeWindow(UPGRADE_WINDOW);

  blocksFileName(CRYPTONOTE_BLOCKS_FILENAME);
  blocksCacheFileName(CRYPTONOTE_BLOCKSCACHE_FILENAME);
  blockIndexesFileName(CRYPTONOTE_BLOCKINDEXES_FILENAME);
  txPoolFileName(CRYPTONOTE_POOLDATA_FILENAME);
  blockchainIndicesFileName(CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME);

  testnet(false);
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
Transaction CurrencyBuilder::generateGenesisTransaction() {
  CryptoNote::Transaction tx;
  CryptoNote::AccountPublicAddress ac = boost::value_initialized<CryptoNote::AccountPublicAddress>();
  m_currency.constructMinerTx(1,0, 0, 0, 0, 0, ac, tx); // zero fee in genesis

  return tx;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
CurrencyBuilder& CurrencyBuilder::emissionSpeedFactor(unsigned int val) {
  if (val <= 0 || val > 8 * sizeof(uint64_t)) {
    throw std::invalid_argument("val at emissionSpeedFactor()");
  }

  m_currency.m_emissionSpeedFactor = val;
  return *this;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
CurrencyBuilder& CurrencyBuilder::numberOfDecimalPlaces(size_t val) {
  m_currency.m_numberOfDecimalPlaces = val;
  m_currency.m_coin = 1;
  for (size_t i = 0; i < m_currency.m_numberOfDecimalPlaces; ++i) {
    m_currency.m_coin *= 10;
  }

  return *this;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
CurrencyBuilder& CurrencyBuilder::difficultyWindow(size_t val) {
  if (val < 2) {
    throw std::invalid_argument("val at difficultyWindow()");
  }

  m_currency.m_difficultyWindow = val;
  return *this;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
CurrencyBuilder& CurrencyBuilder::upgradeVotingThreshold(unsigned int val) {
  if (val <= 0 || val > 100) {
    throw std::invalid_argument("val at upgradeVotingThreshold()");
  }

  m_currency.m_upgradeVotingThreshold = val;
  return *this;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
CurrencyBuilder& CurrencyBuilder::upgradeWindow(size_t val) {
  if (val <= 0) {
    throw std::invalid_argument("val at upgradeWindow()");
  }

  m_currency.m_upgradeWindow = static_cast<uint32_t>(val);
  return *this;
}
//------------------------------------------------------------- Seperator Code -------------------------------------------------------------//
}
