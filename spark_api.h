/**
 * @file spark_api_utils.h
 * @brief 提供调用讯飞星火 API 的接口封装
 */

#ifndef SPARK_API_UTILS_H
#define SPARK_API_UTILS_H

#include <string>
#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include "json.hpp"

// 使用 json 库：https://github.com/nlohmann/json
using json = nlohmann::json;

/**
 * @brief 回调函数用于处理 HTTP 响应内容
 */
static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

/**
 * @brief 调用讯飞星火大模型 API 获取回答
 * @param question 用户提问
 * @return 星火的回答文本
 */
std::string ask_spark_api(const std::string& question) {
    const std::string api_url = "https://spark-api-open.xf-yun.com/v1/chat/completions";
    const std::string api_key = "这里替换成自己的apiPassword";  // ⚠️ 替换成你的APIPassword

    json request_body = {
        {"model", "lite"},
        {"messages", {
            {{"role", "user"}, {"content", question}}
        }},
        {"stream", false}
    };

    CURL* curl = curl_easy_init();
    std::string response_string;
    std::string header_string;

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        std::string post_fields = request_body.dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    try {
    auto json_resp = json::parse(response_string);

    // 检查 code 是否为 0，表示调用成功
    if (json_resp.contains("code") && json_resp["code"] != 0) {
        std::cerr << "星火API错误: " << json_resp["message"] << std::endl;
        return "星火API返回错误：" + json_resp["message"].get<std::string>();
    }

    return json_resp["choices"][0]["message"]["content"];
} catch (const std::exception& e) {
    std::cerr << "JSON解析失败：" << e.what() << std::endl;
    std::cerr << "返回原始字符串：" << response_string << std::endl;
    return "星火API响应异常，可能返回了非法数据。";
}

}

#endif  // SPARK_API_UTILS_H

