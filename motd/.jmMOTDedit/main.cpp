#include <iostream>
#include <curl/curl.h>
#include <json/json.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include <string>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
// this scheiBe took 4 hours to make so i really hope it works - yes it does
// note for my fatass, compile with `g++ motd.cpp -o motd -lssl -lcrypto -lcurl -ljsoncpp

std::string response_data;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    response_data.append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string base64Decode(const std::string& encoded) {
    BIO* bio = BIO_new_mem_buf(encoded.c_str(), -1);
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    char* buffer = new char[encoded.size()];
    int decoded_length = BIO_read(bio, buffer, encoded.size());

    std::string decoded(buffer, decoded_length);
    delete[] buffer;

    BIO_free_all(bio);
    return decoded;
}

std::string base64Encode(const std::string& data) {
    BIO* bio, * b64;
    BUF_MEM* buffer;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, data.c_str(), data.size());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer);

    std::string encoded(buffer->data, buffer->length);
    BIO_free_all(bio);
    return encoded;
}

std::string fetchFileSHA(const std::string& owner, const std::string& repo, const std::string& filePath, const std::string& token) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[e!] failed to initialize CURL.\n";
        return "";
    }

    std::string url = "https://api.github.com/repos/" + owner + "/" + repo + "/contents/" + filePath;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: token " + token).c_str());
    headers = curl_slist_append(headers, "User-Agent: jmMOTDedit");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[CURL/e!] " << curl_easy_strerror(res) << "\n";
        return "";
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;
    std::istringstream s(response_data);
    if (!Json::parseFromStream(reader, s, &root, &errs)) {
        std::cerr << "[e!] failed to parse JSON: " << errs << "\n";
        return "";
    }

    response_data.clear();
    return root["sha"].asString();
}

std::string fetchFileContent(const std::string& owner, const std::string& repo, const std::string& filePath, const std::string& token) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[e!] failed to initialize CURL.\n";
        return "";
    }

    std::string url = "https://api.github.com/repos/" + owner + "/" + repo + "/contents/" + filePath;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: token " + token).c_str());
    headers = curl_slist_append(headers, "User-Agent: CppApp");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[CURL/e!] " << curl_easy_strerror(res) << "\n";
        return "";
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;
    std::istringstream s(response_data);
    if (!Json::parseFromStream(reader, s, &root, &errs)) {
        std::cerr << "[e!] failed to parse JSON: " << errs << "\n";
        return "";
    }

    std::string content = root["content"].asString();
    response_data.clear();
    return base64Decode(content);
}

std::string updateLine(const std::string& content, const std::string& newMotd) {
    std::istringstream stream(content);
    std::string line;
    std::vector<std::string> lines;

    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    if (lines.size() >= 12) {
        lines[11] = "<p>" + newMotd + "</p>";
    } else {
        std::cerr << "[e!] file has fewer than 12 lines!\n";
        return "";
    }

    std::ostringstream updatedContent;
    for (const auto& l : lines) {
        updatedContent << l << "\n";
    }
    return updatedContent.str();
}

void updateFile(const std::string& owner, const std::string& repo, const std::string& filePath,
                const std::string& token, const std::string& sha, const std::string& newContent) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[e!] failed to initialize CURL\n";
        return;
    }

    std::string url = "https://api.github.com/repos/" + owner + "/" + repo + "/contents/" + filePath;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: token " + token).c_str());
    headers = curl_slist_append(headers, "User-Agent: CppApp");
    headers = curl_slist_append(headers, "Content-Type: application/json");

    Json::Value payload;
    payload["message"] = "> update motd";
    payload["content"] = newContent;
    payload["sha"] = sha;

    Json::StreamWriterBuilder writer;
    std::string requestBody = Json::writeString(writer, payload);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[CURL/e!] " << curl_easy_strerror(res) << "\n";
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
}

int main() {                         // basically:
    std::string owner = ""; // you put your github username here e.g. jstmaxlol
    std::string repo = ""; // your repo's name here e.g. jstmaxlol.github.io
    std::string filePath = ""; // the file's name in that repo here e.g. motd/index.html
    std::string token = ""; // your secret fine-grained token here (i obviously removed mine)

    std::string sha = fetchFileSHA(owner, repo, filePath, token);
    if (sha.empty()) {
        std::cerr << "[e!] failed to fetch file SHA\n";
        return 1;
    } std::string content = fetchFileContent(owner, repo, filePath, token);
    if (content.empty()) {
        std::cerr << "[e!] failed to fetch file content\n";
        return 1;
    }
	
	std::string userContent;
	std::cout << "msg: ";
	std::getline(std::cin, userContent);

    std::string updatedContent = updateLine(content, userContent);
    if (updatedContent.empty()) {
        std::cerr << "[e!] failed to update content\n";
        return 1;
    }

    std::string encodedContent = base64Encode(updatedContent);
    updateFile(owner, repo, filePath, token, sha, encodedContent);

    return 0;
}
