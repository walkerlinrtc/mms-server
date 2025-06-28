#include <filesystem>
#include "certs_manager.h"
#include "config/config.h"
#include "spdlog/spdlog.h"
#include "base/utils/utils.h"

using namespace mms;
CertManager::CertManager(Config & top_conf) : top_conf_(top_conf) {

}

CertManager::~CertManager() {

}

void CertManager::start() {

}

int32_t CertManager::add_cert(const std::string & domain, const std::string & key_file, const std::string & cert_file) {
    SSL_CTX * ssl_ctx = SSL_CTX_new(TLSv1_2_method());
    if (!ssl_ctx) {
        spdlog::error("SSL_CTX_new failed!");
        return -1;
    }

    std::shared_ptr<SSL_CTX> ssl_ctx_ptr = std::shared_ptr<SSL_CTX>(ssl_ctx, [](SSL_CTX *ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
    });
    
    if (!std::filesystem::exists(Utils::get_bin_path() + top_conf_.get_cert_root() + "/" + domain + "/" + key_file)) {
        spdlog::error("no key file:{}", Utils::get_bin_path() + top_conf_.get_cert_root() + "/" + domain + "/" + key_file);
        return -2;
    }

    if (!std::filesystem::exists(Utils::get_bin_path() + top_conf_.get_cert_root() + "/" + domain + "/" + cert_file)) {
        spdlog::error("no crt file:{}", Utils::get_bin_path() + top_conf_.get_cert_root() + "/" + domain + "/" + cert_file);
        return -3;
    }

    if (!SSL_CTX_use_PrivateKey_file(ssl_ctx, (Utils::get_bin_path() + top_conf_.get_cert_root() + "/" + domain + "/" + key_file).c_str(), SSL_FILETYPE_PEM)) {
        return -1;
    }

    if (!SSL_CTX_use_certificate_file(ssl_ctx, (Utils::get_bin_path() + top_conf_.get_cert_root() + "/" + domain + "/" + cert_file).c_str(), SSL_FILETYPE_PEM)) {
        return -1;
    }

    std::unique_lock<std::shared_mutex> lck(ssl_ctxs_lck_);
    ssl_ctxs_.insert(std::pair(domain, ssl_ctx_ptr));
    return 0;    
}

std::shared_ptr<SSL_CTX> CertManager::get_cert_ctx(const std::string & domain) {
    std::shared_lock<std::shared_mutex> lck(ssl_ctxs_lck_);
    auto it = ssl_ctxs_.find(domain);
    if (it == ssl_ctxs_.end()) {
        return nullptr;
    }
    return it->second;
}

void CertManager::stop() {

}

