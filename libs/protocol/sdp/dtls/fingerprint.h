// SDP中fingerprint的作用
// sdp信息中会出现如下字段

// a=fingerprint:sha-256 5D:60:1C:B7:B3:A7:C6:32:E8:6D:54:80:00:4B:26:0A:A1:62:CB:57:79:83:2D:69:A6:D9:B9:28:6A:77:71:C7
// a=setup:actpass
// 1
// 2
// 那么fingerprint是在什么时候使用呢，翻阅mediasoup代码可以看到，在创建dtls连接的时候，会根据客户端建立的ssl获取到其证书，获取到证书之后，通过sdp中的加密算法，推算出对应的签名值，并与sdp中值进行比较，如果不对，那么证书被篡改，链接失效

// 代码如下：
// ————————————————
// 版权声明：本文为CSDN博主「slionercheng」的原创文章，遵循CC 4.0 BY-SA版权协议，转载请附上原文出处链接及本声明。
// 原文链接：https://blog.csdn.net/qq_27031005/article/details/112689200

// WebRTC 中 DTLS 参数
// WebRTC 中的 DTLS 参数需要通过 SDP 信息来设置和传递，而且只需要两种 a line 表示：

// a=setup
// a=fingerprint

// a=setup 表示 DTLS 的协商过程中角色，有三个可能值：

// a=setup:actpass 既可以是 client 角色，也可以是 server 角色
// a=setup:active client 角色
// a=setup:passive server 角色
// 根据 RFC5763，SDP请求的 a=setup 属性必须是 actpass，由应答方决定谁是 DTLS client，谁是 DTLS server。

// a=fingerprint 的内容是证书的摘要签名，用于验证证书的有效性，防止冒充。
// ————————————————
// 版权声明：本文为CSDN博主「大飞飞鱼」的原创文章，遵循CC 4.0 BY-SA版权协议，转载请附上原文出处链接及本声明。
// 原文链接：https://blog.csdn.net/ababab12345/article/details/122151025

#pragma once
#include <string>
namespace mms {
struct FingerPrint {
public:
    static std::string prefix;
    bool parse(const std::string & line);
    FingerPrint() = default;
    FingerPrint(const std::string & hash_name, const std::string & hash_val) : hash_name_(hash_name), hash_val_(hash_val) {

    }

    const std::string & get_hash_name() const {
        return hash_name_;
    }

    void set_hash_name(const std::string & val) {
        hash_name_ = val;
    }

    const std::string & get_hash_val() const {
        return hash_val_;
    }

    void set_hash_val(const std::string & val) {
        hash_val_ = val;
    }

    std::string to_string() const;
private:
    std::string hash_name_;
    std::string hash_val_;
};
};