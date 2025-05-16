#include "spdlog/spdlog.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string.h>
#include <openssl/x509v3.h>

#include "dtls_cert.h"
using namespace mms;
bool DtlsCert::init(const std::string & domain)
{
    domain_ = domain;
    if (!create_cert())
    {
        return false;
    }

    return true;
}

DtlsCert::~DtlsCert()
{
    // once you've successfully called EVP_PKEY_assign_RSA() you must not call RSA_free() on the underlying key or a double-free may result.
    // if (rsa_)
    // {
    //     RSA_free(rsa_);
    //     rsa_ = nullptr;
    // }

    if (bn_) {
        BN_free(bn_);
        bn_ = nullptr;
    }

    if (pkey_) {
        EVP_PKEY_free(pkey_);
        pkey_ = nullptr;
    }

    if (certificate_)
    {
        X509_free(certificate_);
        certificate_ = nullptr;
    }
}

bool check_certificate_valid(X509 *x509)
{
    X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    X509_STORE *store = X509_STORE_new();

    X509_STORE_add_cert(store, x509);
    X509_STORE_CTX_init(ctx, store, x509, nullptr);

    return X509_verify_cert(ctx) == 1 ? true : false;
}

int add_ext(X509 *cert, int nid, char *value)
{
    (void)cert;
    (void)nid;
    (void)value;
    // X509_EXTENSION *ex;
    // X509V3_CTX ctx;
    // /* This sets the 'context' of the extensions. */
    // /* No configuration database */
    // X509V3_set_ctx_nodb(&ctx);
    // /* Issuer and subject certs: both the target since it is self signed,
    //  * no request and no CRL
    //  */
    // X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
    // ex = X509V3_EXT_conf_nid(NULL, &ctx, nid, value);
    // if (!ex)
    //     return 0;

    // X509_add_ext(cert, ex, -1);
    // X509_EXTENSION_free(ex);
    return 1;
}

#define RSA_KEY_LENGTH 512
bool DtlsCert::create_cert()
{
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    // 创建证书
    certificate_ = X509_new();
    X509_set_version(certificate_, 2);
    // 生成随机序号
    std::srand(time(nullptr));
    int seq = std::rand() % 9999999;
    ASN1_INTEGER_set(X509_get_serialNumber(certificate_), seq); // serial number
    // 创建pkey
    rsa_ = RSA_new();
    bn_ = BN_new();
    pkey_ = EVP_PKEY_new();
    BN_set_word(bn_, RSA_F4);
    int ret = RSA_generate_key_ex(rsa_, RSA_KEY_LENGTH, bn_, nullptr);
    if (ret != 1)
    {
        BN_free(bn_);bn_ = nullptr;
        EVP_PKEY_free(pkey_);pkey_ = nullptr;
        return false;
    }
    EVP_PKEY_assign(pkey_, EVP_PKEY_RSA, reinterpret_cast<char *>(rsa_));
    X509_set_pubkey(certificate_, pkey_);
    // 设置时长
    static const int expired_days = 365 * 10;
    static const long int expired_time_after = 60 * 60 * 24 * expired_days;
    static const long int expired_time_before = -1 * expired_days;
    X509_gmtime_adj(X509_get_notBefore(certificate_), expired_time_before); // now
    X509_gmtime_adj(X509_get_notAfter(certificate_), expired_time_after);   // accepts secs
    // 设置域名等信息
    // 1 -- X509_NAME may disambig with wincrypt.h
    // 2 -- DO NO FREE the name internal pointer
    X509_NAME *name = X509_get_subject_name(certificate_);
    // const u_char country[] = "China";
    // const u_char company[] = "mms, PLC";
    // const u_char common_name[] = "localhost";
    // X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, country, -1, -1, 0);
    // X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, company, -1, -1, 0);
    // X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, common_name, -1, -1, 0);

    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, (const unsigned char *)"China", -1, -1, 0); //country
    X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (const unsigned char *)"SG", -1, -1, 0); //state
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)domain_.c_str(), -1, -1, 0); //common name
    X509_set_issuer_name(certificate_, name);
    // 添加ext
    /* Add various extensions: standard extensions */
	// add_ext(certificate_, NID_basic_constraints, (char*)("critical,CA:TRUE"));
	// add_ext(certificate_, NID_key_usage, (char*)("critical,keyCertSign,cRLSign"));
	// add_ext(certificate_, NID_subject_key_identifier, (char*)("hash"));
    // add_ext(certificate_, NID_authority_key_identifier, (char*)("keyid:always"));
    // 生成签名
    ret = X509_sign(certificate_, pkey_, EVP_sha1()); // some hash type here
    if (ret == 0)
    {
        BN_free(bn_);bn_ = nullptr;
        EVP_PKEY_free(pkey_);pkey_ = nullptr;
        return false;
    }
    // 生成finger print
    std::unique_ptr<BIO, void (*)(BIO *)> memIO(BIO_new(BIO_s_mem()), BIO_free_all);
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int n;
    X509_digest(certificate_, EVP_sha1(), md, &n);
    std::ostringstream oss;
    for (unsigned int pos = 0; pos < n; pos++)
    {
        if (pos != n - 1)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)md[pos] << ":";
        }
        else
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)md[pos];
        }
    }
    finger_print_ = oss.str();
    // 写入der
    std::unique_ptr<BIO, void (*)(BIO *)> memDER{BIO_new(BIO_s_mem()), BIO_free_all};
    ret = i2d_X509_bio(memDER.get(), certificate_);
    if (ret != 1)
    {
        BN_free(bn_);bn_= nullptr;
        EVP_PKEY_free(pkey_);pkey_ = nullptr;
        return false;
    }

    char *der_ptr = NULL;
    long der_len = BIO_get_mem_data(memDER.get(), &der_ptr);
    der_.resize(der_len);
    memcpy(der_.data(), der_ptr, der_len);
    // key 写入文件，调试用
    std::unique_ptr<BIO, void (*)(BIO *)> keyFile{BIO_new_file("./mms.rsa", "wb"), BIO_free_all};
    PEM_write_bio_PrivateKey(keyFile.get(), pkey_, nullptr, nullptr, 0, nullptr, nullptr);
    return true;
}

RSA * DtlsCert::get_rsa() {
    return rsa_;
}