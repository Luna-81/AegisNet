#include "src/base/AsyncLogging.h"
#include "src/base/LogFile.h"
#include "src/base/LogStream.h"

#include "test_support.h"

#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    {
        FixedBuffer<8> buffer;
        REQUIRE(buffer.append("12345678", 8));
        REQUIRE(buffer.length() == 8);
        REQUIRE(!buffer.append("x", 1));
        REQUIRE(buffer.length() == 8);
    }

    {
        LogStream stream;
        stream << std::string(10000, 'x');
        const auto& buffer = stream.buffer();
        const std::string text(buffer.data(), buffer.length());
        REQUIRE(buffer.length() <= detail::kSmallBuffer);
        REQUIRE(text.find("[truncated]\n") != std::string::npos);
    }

    const auto directory = std::filesystem::temp_directory_path() /
                           ("aegisnet-logging-test-" +
                            std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    const std::string rollingBase = (directory / "rolling").string();
    {
        LogFile rollingFile(rollingBase, 4);
        rollingFile.append("1234", 4);
        rollingFile.append("x", 1);
        rollingFile.flush();
    }

    std::vector<std::filesystem::path> rollingFiles;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().filename().string().rfind("rolling.", 0) == 0) {
            rollingFiles.push_back(entry.path());
        }
    }
    std::sort(rollingFiles.begin(), rollingFiles.end());
    REQUIRE(rollingFiles.size() == 2);
    REQUIRE(rollingFiles[0].filename() != rollingFiles[1].filename());
    REQUIRE(readFile(rollingFiles[0]) == "1234");
    REQUIRE(readFile(rollingFiles[1]) == "x");

    const std::string asyncBase = (directory / "async").string();
    const std::string tail = "final record before immediate stop\n";
    const std::string largeRecord(detail::kLargeBuffer + 37, 'z');
    {
        AsyncLogging logging(asyncBase, 1);
        logging.start();
        logging.append(tail.data(), static_cast<int>(tail.size()));
        logging.append(largeRecord.data(), static_cast<int>(largeRecord.size()));
        logging.stop();
    }

    std::vector<std::filesystem::path> asyncFiles;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().filename().string().rfind("async.", 0) == 0) {
            asyncFiles.push_back(entry.path());
        }
    }
    REQUIRE(asyncFiles.size() == 1);
    REQUIRE(readFile(asyncFiles.front()) == tail + largeRecord);

    const std::string concurrentBase = (directory / "concurrent").string();
    {
        AsyncLogging logging(concurrentBase, 1);
        logging.start();
        std::vector<std::thread> producers;
        for (int threadIndex = 0; threadIndex < 4; ++threadIndex) {
            producers.emplace_back([&logging, threadIndex]() {
                for (int recordIndex = 0; recordIndex < 250; ++recordIndex) {
                    const std::string record = std::to_string(threadIndex) + ":" +
                                               std::to_string(recordIndex) + "\n";
                    logging.append(record.data(), static_cast<int>(record.size()));
                }
            });
        }
        for (auto& producer : producers) producer.join();
        logging.stop();
    }

    std::vector<std::filesystem::path> concurrentFiles;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().filename().string().rfind("concurrent.", 0) == 0) {
            concurrentFiles.push_back(entry.path());
        }
    }
    REQUIRE(concurrentFiles.size() == 1);
    std::istringstream records(readFile(concurrentFiles.front()));
    std::set<std::string> uniqueRecords;
    std::string record;
    while (std::getline(records, record)) uniqueRecords.insert(record);
    REQUIRE(uniqueRecords.size() == 1000);

    std::filesystem::remove_all(directory);
}
