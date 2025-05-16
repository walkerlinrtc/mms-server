#pragma once
#include <string>
#include <memory>
#include <stdio.h>

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/x509.h"

namespace mms
{
    class DtlsCert
    {
    public:
        DtlsCert() = default;
        ~DtlsCert();
        bool init(const std::string & domain);

        const std::string &get_finger_print() const
        {
            return finger_print_;
        }

        const std::string &get_der() const {
            return der_;
        }

        RSA * get_rsa();

        X509 * get_cert() {
            return certificate_;
        }

        EVP_PKEY * get_pkey() {
            return pkey_;
        }        
    private:
        std::string domain_;
        X509 *certificate_ = nullptr;
        RSA *rsa_ = nullptr;
        EVP_PKEY *pkey_ = nullptr;
        BIGNUM *bn_ = nullptr;
        std::string finger_print_;
        std::string der_;
    private:
        bool create_cert();
    };
};