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
#include "strategymanager.h"
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
    kConfig.fundingRateThreshold =
        config->getDouble("order.fundingRateThreshold");
    kConfig.openPrincipal = config->getDouble("order.openPrincipal");

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

  welcome();
  NOTICE("START PID " << getpid());
  NOTICE_("api", "START PID " << getpid());

  float availBal = 0;
  Bitget::assets(availBal);
  INFO("availBal " << availBal);

  auto monitor = std::make_shared<FundingRateMonitor>(60);
  monitor->start();

  auto manager = std::make_shared<StrategyManager>(monitor);
  manager->start();

  while (true) {
    SLEEP_MS(1000);
  }

  manager->stop();
  monitor->stop();

  curl_global_cleanup();
  NOTICE("STOP PID " << getpid());
  NOTICE_("api", "STOP PID " << getpid());
  bye();
  return 0;
}
