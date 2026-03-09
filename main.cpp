#include <curl/curl.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Poco/AutoPtr.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "bitget.h"
#include "fundingratemonitor.h"
#include "tracer.h"
#include "util.h"

Config kConfig;
using Poco::AutoPtr;
using Poco::NotFoundException;
using Poco::Util::AbstractConfiguration;
using Poco::Util::PropertyFileConfiguration;

void welcome() {
  NOTICE(R"( _     _ _             _    )");
  NOTICE(R"(| |__ (_) |_ __ _  ___| |_  )");
  NOTICE(R"(| '_ \| | __/ _` |/ _ \ __| )");
  NOTICE(R"(| |_) | | || (_| |  __/ |_  )");
  NOTICE(R"(|_.__/|_|\__\__, |\___|\__| )");
  NOTICE(R"(            |___/           )");
}

void bye() {
  NOTICE(R"( ____   __   __  _____     )");
  NOTICE(R"(| __ )  \ \ / / | ____|    )");
  NOTICE(R"(|  _ \   \ V /  |  _|      )");
  NOTICE(R"(| |_) |   | |   | |___     )");
  NOTICE(R"(|____/    |_|   |_____|    )");
  NOTICE(R"(                           )");
}

void InitConfig() {
  try {
    Poco::File dir("log");
    if (!dir.exists()) {
      dir.createDirectory();
    }
    AutoPtr<PropertyFileConfiguration> config =
        new PropertyFileConfiguration("config.properties");
    kConfig.lever = config->getDouble("order.lever");
    kConfig.fundingRateThreshold = config->getDouble("order.fundingRateThreshold");

    kConfig.apiKey = config->getString("bitget.apiKey");
    kConfig.secretKey = config->getString("bitget.secretKey");
    kConfig.passphrase = config->getString("bitget.passphrase");
    Bitget::setApiParam();

    kConfig.logName = config->getString("log.logName");
    kConfig.logSize = config->getString("log.logSize");
    kConfig.logLevel = config->getString("log.logLevel");
    logger::Tracer::Init("default", kConfig.logName, kConfig.logSize);
    logger::Tracer::Init("api", "log/api.log", kConfig.logSize);
    logger::Tracer::SetLevel(kConfig.logLevel);

    kConfig.barkServer = config->getString("bark.server");
  } catch (NotFoundException& e) {
    std::cerr << "Config not found: " << e.what() << std::endl;
    exit(-1);
  }
}

int main(int argc, char* argv[]) {
  InitConfig();
  curl_global_init(CURL_GLOBAL_DEFAULT);

  auto fundingRateMonitor = std::make_shared<FundingRateMonitor>();
  fundingRateMonitor->start();

  welcome();
  NOTICE("START PID " << getpid());
  NOTICE_("api", "START PID " << getpid());

  float availBal = 0;
  Bitget::assets(availBal);
  INFO("availBal " << availBal);

  // Hedge test: DOGE crossed-margin long + futures short, both 2x.
  const std::string hedgeSymbol = "DOGEUSDT";
  auto futuresClient = std::make_shared<Bitget>(hedgeSymbol);
  futuresClient->setLeverage(2);

  Ticker ticker;
  if (futuresClient->tickers(ticker) && ticker.lastPr > 0) {
    float minBaseSize = safeStof(futuresClient->getSymbol().minTradeNum);
    if (minBaseSize > 0) {
      minBaseSize = std::stof(
          adjustDecimalPlaces(minBaseSize, futuresClient->getSymbol().volumePlace));
      float minNotional = minBaseSize * ticker.lastPr;

      MarginOrder marginOrder;
      marginOrder.symbol = hedgeSymbol;
      marginOrder.side = "buy";
      marginOrder.orderType = "market";
      marginOrder.loanType = "autoLoan";
      marginOrder.quoteSize = safeFtos(minNotional, futuresClient->precision());

      Order futuresOrder;
      futuresOrder.symbol = hedgeSymbol;
      futuresOrder.side = "sell";
      futuresOrder.size = minBaseSize;

      bool marginOk = futuresClient->placeMarginOrder(marginOrder);
      bool futuresOk = futuresClient->placeMarketOrder(futuresOrder);

      NOTICE("Hedge test result marginLong=" << marginOk
             << " futuresShort=" << futuresOk << " symbol=" << hedgeSymbol
             << " baseSize=" << minBaseSize << " notional=" << minNotional);

      if (marginOk && futuresOk) {
        CrossedMarginAsset dogeAsset;
        Position futuresPosition;
        bool marginAssetOk = Bitget::crossedMarginAsset("DOGE", dogeAsset);
        bool futuresPosOk = futuresClient->singlePosition(futuresPosition);

        if (marginAssetOk && futuresPosOk) {
          float marginDogeQty =
              dogeAsset.totalAmount > 0 ? dogeAsset.totalAmount : dogeAsset.net;
          float futuresDogeQty = futuresPosition.total;
          float diff = std::fabs(marginDogeQty - futuresDogeQty);
          bool hedgeMatched = diff <= minBaseSize;

          NOTICE("Hedge check DOGE marginQty=" << marginDogeQty
                 << " futuresQty=" << futuresDogeQty << " diff=" << diff
                 << " threshold=" << minBaseSize
                 << " matched=" << hedgeMatched);
        } else {
          ERROR("Hedge check failed: marginAssetOk=" << marginAssetOk
                << " futuresPosOk=" << futuresPosOk);
        }
      }
    } else {
      ERROR("Hedge test skipped: invalid minTradeNum for " << hedgeSymbol);
    }
  } else {
    ERROR("Hedge test skipped: failed to fetch ticker for " << hedgeSymbol);
  }

  // Keep monitor running for debugging and print top funding rates from main.
  while (true) {
    auto allFundingRates = fundingRateMonitor->getAllFundingRates();
    if (!allFundingRates.empty()) {
      std::sort(allFundingRates.begin(), allFundingRates.end(),
                [](const FundingRate& left, const FundingRate& right) {
                  return std::fabs(left.rate) > std::fabs(right.rate);
                });

      NOTICE("All " << allFundingRates.size()
                     << " symbols by |fundingRate| (from main)");
      for (size_t index = 0; index < allFundingRates.size(); ++index) {
        const auto& fr = allFundingRates[index];
        NOTICE("#" << (index + 1) << " " << fr.symbol << " rate=" << fr.rate
                   << " interval=" << fr.fundingRateInterval << "h"
                   << " nextUpdate=" << fr.nextFundingTime);
      }
    }
    SLEEP_MS(60 * 1000);
  }

  fundingRateMonitor->stop();

  curl_global_cleanup();
  NOTICE("STOP PID " << getpid());
  NOTICE_("api", "STOP PID " << getpid());
  bye();
  return 0;
}
