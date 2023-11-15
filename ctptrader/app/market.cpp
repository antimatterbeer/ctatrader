#include <app/market.hpp>

namespace ctptrader::app {

void MdSpi::OnFrontConnected() {
  ctx_->GetLogger()->info("Connected to market server, sending login request");
  CThostFtdcReqUserLoginField req{};
  memset(&req, 0, sizeof(req));
  strcpy(req.BrokerID, broker_id_.c_str());
  strcpy(req.UserID, user_id_.c_str());
  strcpy(req.Password, password_.c_str());
  if (api_->ReqUserLogin(&req, 0) != 0) {
    ctx_->GetLogger()->error("Sending login request failed");
  } else {
    ctx_->GetLogger()->info("Sending login request succeeded");
  }
}

void MdSpi::OnFrontDisconnected(int nReason) {
  ctx_->GetLogger()->info("Disconnected from market server, reason: {}", nReason);
}

void MdSpi::OnHeartBeatWarning(int nTimeLapse) {
  ctx_->GetLogger()->info("Heartbeat warning, time lapse: {}", nTimeLapse);
}

void MdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
                           CThostFtdcRspInfoField *pRspInfo,
                           [[maybe_unused]] int nRequestID,
                           [[maybe_unused]] bool bIsLast) {
  if (pRspInfo->ErrorID == 0) {
    ctx_->GetLogger()->info("Login succeeded. Trading day: {}",
                         pRspUserLogin->TradingDay);
    std::vector<char *> instruments;
    for (size_t i = 0; i < interests_.size(); ++i) {
      if (interests_[i] == 1) {
        auto name = ctx_->GetInstrumentCenter().Get(i).name_;
        instruments.push_back(const_cast<char *>(name.c_str()));
      }
    }
    api_->SubscribeMarketData(instruments.data(), instruments.size());
  } else {
    ctx_->GetLogger()->error("Login failed, error id: {}, error message: {}",
                          pRspInfo->ErrorID, pRspInfo->ErrorMsg);
  }
}

void MdSpi::OnRspUserLogout(
    [[maybe_unused]] CThostFtdcUserLogoutField *pUserLogout,
    CThostFtdcRspInfoField *pRspInfo, [[maybe_unused]] int nRequestID,
    [[maybe_unused]] bool bIsLast) {
  if (pRspInfo->ErrorID == 0) {
    ctx_->GetLogger()->info("Logout succeeded");
  } else {
    ctx_->GetLogger()->error("Logout failed, error id: {}, error message: {}",
                          pRspInfo->ErrorID, pRspInfo->ErrorMsg);
  }
}

void MdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo,
                       [[maybe_unused]] int nRequestID,
                       [[maybe_unused]] bool bIsLast) {
  ctx_->GetLogger()->error("Error, error id: {}, error message: {}",
                        pRspInfo->ErrorID, pRspInfo->ErrorMsg);
}

void MdSpi::OnRspSubMarketData(
    CThostFtdcSpecificInstrumentField *pSpecificInstrument,
    CThostFtdcRspInfoField *pRspInfo, [[maybe_unused]] int nRequestID,
    [[maybe_unused]] bool bIsLast) {
  ctx_->GetLogger()->trace(
      "Subscribing market data response received. request id: {}, is last: {}",
      nRequestID, bIsLast);
  if (pRspInfo->ErrorID == 0) {
    ctx_->GetLogger()->info("Subscribing market data succeeded, instrument id: {}",
                         pSpecificInstrument->InstrumentID);
  } else {
    ctx_->GetLogger()->error(
        "Subscribing market data failed, error id: {}, error message: {}",
        pRspInfo->ErrorID, pRspInfo->ErrorMsg);
  }
}

void MdSpi::OnRspUnSubMarketData(
    CThostFtdcSpecificInstrumentField *pSpecificInstrument,
    CThostFtdcRspInfoField *pRspInfo, [[maybe_unused]] int nRequestID,
    [[maybe_unused]] bool bIsLast) {
  if (pRspInfo->ErrorID == 0) {
    ctx_->GetLogger()->info(
        "Unsubscribing market data succeeded, instrument id: {}",
        pSpecificInstrument->InstrumentID);
  } else {
    ctx_->GetLogger()->error(
        "Unsubscribing market data failed, error id: {}, error message: {}",
        pRspInfo->ErrorID, pRspInfo->ErrorMsg);
  }
}

void MdSpi::OnRspSubForQuoteRsp(
    CThostFtdcSpecificInstrumentField *pSpecificInstrument,
    CThostFtdcRspInfoField *pRspInfo, [[maybe_unused]] int nRequestID,
    [[maybe_unused]] bool bIsLast) {
  if (pRspInfo->ErrorID == 0) {
    ctx_->GetLogger()->info("Subscribing quote data succeeded, instrument id: {}",
                         pSpecificInstrument->InstrumentID);
  } else {
    ctx_->GetLogger()->error(
        "Subscribing quote data failed, error id: {}, error message: {}",
        pRspInfo->ErrorID, pRspInfo->ErrorMsg);
  }
}

void MdSpi::OnRspUnSubForQuoteRsp(
    CThostFtdcSpecificInstrumentField *pSpecificInstrument,
    CThostFtdcRspInfoField *pRspInfo, [[maybe_unused]] int nRequestID,
    [[maybe_unused]] bool bIsLast) {
  if (pRspInfo->ErrorID == 0) {
    ctx_->GetLogger()->info(
        "Unsubscribing quote data succeeded, instrument id: {}",
        pSpecificInstrument->InstrumentID);
  } else {
    ctx_->GetLogger()->error(
        "Unsubscribing quote data failed, error id: {}, error message: {}",
        pRspInfo->ErrorID, pRspInfo->ErrorMsg);
  }
}

void MdSpi::OnRtnDepthMarketData(
    CThostFtdcDepthMarketDataField *pDepthMarketData) {
  auto id = ctx_->GetInstrumentCenter().GetID(pDepthMarketData->InstrumentID);
  if (!received_[id] == 0) {
    static_.id_ = id;
    // static_.trading_day_ =
    // base::Date::FromString(pDepthMarketData->TradingDay);
    static_.prev_close_ = pDepthMarketData->PreClosePrice;
    static_.upper_limit_ = pDepthMarketData->UpperLimitPrice;
    static_.lower_limit_ = pDepthMarketData->LowerLimitPrice;
    if (!tx_.Write(static_)) {
      ctx_->GetLogger()->error("Failed to write static data to tx");
    }
    received_[id] = 1;
  }

  depth_.id_ = id;
  depth_.open_ = pDepthMarketData->OpenPrice;
  depth_.high_ = pDepthMarketData->HighestPrice;
  depth_.low_ = pDepthMarketData->LowestPrice;
  depth_.last_ = pDepthMarketData->ClosePrice;
  depth_.open_interest_ = pDepthMarketData->OpenInterest;
  depth_.volume_ = pDepthMarketData->Volume;
  depth_.turnover_ = pDepthMarketData->Turnover;
  depth_.ask_price_[0] = pDepthMarketData->AskPrice1;
  depth_.bid_price_[0] = pDepthMarketData->BidPrice1;
  depth_.ask_volume_[0] = pDepthMarketData->AskVolume1;
  depth_.bid_volume_[0] = pDepthMarketData->BidVolume1;
  if (!tx_.Write(depth_)) {
    ctx_->GetLogger()->error("Failed to write depth data to tx");
  }
}

void MdSpi::SetInterests(std::vector<std::string> instruments) {
  interests_.assign(ctx_->GetInstrumentCenter().Count(), 0);
  received_.assign(ctx_->GetInstrumentCenter().Count(), 0);
  for (auto &i : instruments) {
    auto id = ctx_->GetInstrumentCenter().GetID(i);
    if (id >= 0) {
      interests_[id] = 1;
    }
  }
}

} // namespace ctptrader::app