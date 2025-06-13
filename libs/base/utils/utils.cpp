/*
 * @Author: jbl19860422
 * @Date: 2023-08-22 07:01:05
 * @LastEditTime: 2023-08-22 07:01:20
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\base\utils\utils.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <sys/time.h>
#include <chrono>
#include <ctime>
#include <string>
// #include <linux/time.h>



#include <unistd.h>
#include <string.h>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/evp.h>

#include <iostream>
#include <sstream>
#include <string_view>
#include <array>
#include <filesystem>
#include <regex>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

#include "utils.h"

using namespace mms;
uint64_t Utils::get_rand64()
{
    srand(time(NULL));
    int64_t rand_data = 0;
    for (int i = 0; i < 8; i++)
    {
        int64_t mask = 0xFF;
        mask = mask << (i * 8);
        int bdata = ((mask) >> (i * 8)) & 0xFF;
        if (bdata > 0)
        {
            int64_t data = rand() % bdata;
            rand_data |= data << (i * 8);
        }
    }
    return rand_data;
}

std::string Utils::get_rand_str(size_t len)
{
    static const std::string table = "abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string ret;
    ret.resize(len);
    for (size_t i = 0; i < len; ++i)
    {
        ret[i] = table[std::rand() % table.size()];
    }
    return ret;
}

const uint32_t crc_table[] = {
    0x00000000,
    0x77073096,
    0xee0e612c,
    0x990951ba,
    0x076dc419,
    0x706af48f,
    0xe963a535,
    0x9e6495a3,
    0x0edb8832,
    0x79dcb8a4,
    0xe0d5e91e,
    0x97d2d988,
    0x09b64c2b,
    0x7eb17cbd,
    0xe7b82d07,
    0x90bf1d91,
    0x1db71064,
    0x6ab020f2,
    0xf3b97148,
    0x84be41de,
    0x1adad47d,
    0x6ddde4eb,
    0xf4d4b551,
    0x83d385c7,
    0x136c9856,
    0x646ba8c0,
    0xfd62f97a,
    0x8a65c9ec,
    0x14015c4f,
    0x63066cd9,
    0xfa0f3d63,
    0x8d080df5,
    0x3b6e20c8,
    0x4c69105e,
    0xd56041e4,
    0xa2677172,
    0x3c03e4d1,
    0x4b04d447,
    0xd20d85fd,
    0xa50ab56b,
    0x35b5a8fa,
    0x42b2986c,
    0xdbbbc9d6,
    0xacbcf940,
    0x32d86ce3,
    0x45df5c75,
    0xdcd60dcf,
    0xabd13d59,
    0x26d930ac,
    0x51de003a,
    0xc8d75180,
    0xbfd06116,
    0x21b4f4b5,
    0x56b3c423,
    0xcfba9599,
    0xb8bda50f,
    0x2802b89e,
    0x5f058808,
    0xc60cd9b2,
    0xb10be924,
    0x2f6f7c87,
    0x58684c11,
    0xc1611dab,
    0xb6662d3d,
    0x76dc4190,
    0x01db7106,
    0x98d220bc,
    0xefd5102a,
    0x71b18589,
    0x06b6b51f,
    0x9fbfe4a5,
    0xe8b8d433,
    0x7807c9a2,
    0x0f00f934,
    0x9609a88e,
    0xe10e9818,
    0x7f6a0dbb,
    0x086d3d2d,
    0x91646c97,
    0xe6635c01,
    0x6b6b51f4,
    0x1c6c6162,
    0x856530d8,
    0xf262004e,
    0x6c0695ed,
    0x1b01a57b,
    0x8208f4c1,
    0xf50fc457,
    0x65b0d9c6,
    0x12b7e950,
    0x8bbeb8ea,
    0xfcb9887c,
    0x62dd1ddf,
    0x15da2d49,
    0x8cd37cf3,
    0xfbd44c65,
    0x4db26158,
    0x3ab551ce,
    0xa3bc0074,
    0xd4bb30e2,
    0x4adfa541,
    0x3dd895d7,
    0xa4d1c46d,
    0xd3d6f4fb,
    0x4369e96a,
    0x346ed9fc,
    0xad678846,
    0xda60b8d0,
    0x44042d73,
    0x33031de5,
    0xaa0a4c5f,
    0xdd0d7cc9,
    0x5005713c,
    0x270241aa,
    0xbe0b1010,
    0xc90c2086,
    0x5768b525,
    0x206f85b3,
    0xb966d409,
    0xce61e49f,
    0x5edef90e,
    0x29d9c998,
    0xb0d09822,
    0xc7d7a8b4,
    0x59b33d17,
    0x2eb40d81,
    0xb7bd5c3b,
    0xc0ba6cad,
    0xedb88320,
    0x9abfb3b6,
    0x03b6e20c,
    0x74b1d29a,
    0xead54739,
    0x9dd277af,
    0x04db2615,
    0x73dc1683,
    0xe3630b12,
    0x94643b84,
    0x0d6d6a3e,
    0x7a6a5aa8,
    0xe40ecf0b,
    0x9309ff9d,
    0x0a00ae27,
    0x7d079eb1,
    0xf00f9344,
    0x8708a3d2,
    0x1e01f268,
    0x6906c2fe,
    0xf762575d,
    0x806567cb,
    0x196c3671,
    0x6e6b06e7,
    0xfed41b76,
    0x89d32be0,
    0x10da7a5a,
    0x67dd4acc,
    0xf9b9df6f,
    0x8ebeeff9,
    0x17b7be43,
    0x60b08ed5,
    0xd6d6a3e8,
    0xa1d1937e,
    0x38d8c2c4,
    0x4fdff252,
    0xd1bb67f1,
    0xa6bc5767,
    0x3fb506dd,
    0x48b2364b,
    0xd80d2bda,
    0xaf0a1b4c,
    0x36034af6,
    0x41047a60,
    0xdf60efc3,
    0xa867df55,
    0x316e8eef,
    0x4669be79,
    0xcb61b38c,
    0xbc66831a,
    0x256fd2a0,
    0x5268e236,
    0xcc0c7795,
    0xbb0b4703,
    0x220216b9,
    0x5505262f,
    0xc5ba3bbe,
    0xb2bd0b28,
    0x2bb45a92,
    0x5cb36a04,
    0xc2d7ffa7,
    0xb5d0cf31,
    0x2cd99e8b,
    0x5bdeae1d,
    0x9b64c2b0,
    0xec63f226,
    0x756aa39c,
    0x026d930a,
    0x9c0906a9,
    0xeb0e363f,
    0x72076785,
    0x05005713,
    0x95bf4a82,
    0xe2b87a14,
    0x7bb12bae,
    0x0cb61b38,
    0x92d28e9b,
    0xe5d5be0d,
    0x7cdcefb7,
    0x0bdbdf21,
    0x86d3d2d4,
    0xf1d4e242,
    0x68ddb3f8,
    0x1fda836e,
    0x81be16cd,
    0xf6b9265b,
    0x6fb077e1,
    0x18b74777,
    0x88085ae6,
    0xff0f6a70,
    0x66063bca,
    0x11010b5c,
    0x8f659eff,
    0xf862ae69,
    0x616bffd3,
    0x166ccf45,
    0xa00ae278,
    0xd70dd2ee,
    0x4e048354,
    0x3903b3c2,
    0xa7672661,
    0xd06016f7,
    0x4969474d,
    0x3e6e77db,
    0xaed16a4a,
    0xd9d65adc,
    0x40df0b66,
    0x37d83bf0,
    0xa9bcae53,
    0xdebb9ec5,
    0x47b2cf7f,
    0x30b5ffe9,
    0xbdbdf21c,
    0xcabac28a,
    0x53b39330,
    0x24b4a3a6,
    0xbad03605,
    0xcdd70693,
    0x54de5729,
    0x23d967bf,
    0xb3667a2e,
    0xc4614ab8,
    0x5d681b02,
    0x2a6f2b94,
    0xb40bbe37,
    0xc30c8ea1,
    0x5a05df1b,
    0x2d02ef8d,
};

uint32_t Utils::get_crc32(uint8_t *buf, size_t len)
{
    if (len < 1)
        return 0xffffffff;

    uint32_t crc = 0xffffffff;

    for (size_t i = 0; i != len; ++i)
    {
        crc = crc_table[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);
    }

    crc = crc ^ 0xffffffff;

    return crc;
}

const uint32_t mpegts_crc_table[] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4,
};

uint32_t Utils::calc_mpeg_ts_crc32(uint8_t *buf, size_t len) {
	uint32_t crc = 0xffffffff;
	for (size_t i = 0; i < len; i++) {
		crc = (crc << 8) ^ mpegts_crc_table[((crc>>24)^uint32_t(buf[i]))&0xff];
	}

	return crc;
}

std::string Utils::calc_hmac_sha256(const std::string &key, const std::string &msg)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
    unsigned int hashLen;

    HMAC(
        EVP_sha256(),
        key.data(),
        static_cast<int>(key.size()),
        reinterpret_cast<unsigned char const *>(msg.data()),
        static_cast<int>(msg.size()),
        hash.data(),
        &hashLen);

    return std::string{reinterpret_cast<char const *>(hash.data()), hashLen};
}

std::string Utils::calc_hmac_sha1(const std::string &key, const std::string &msg)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
    unsigned int hashLen;

    HMAC(
        EVP_sha1(),
        key.data(),
        static_cast<int>(key.size()),
        reinterpret_cast<unsigned char const *>(msg.data()),
        static_cast<int>(msg.size()),
        hash.data(),
        &hashLen);

    return std::string{reinterpret_cast<char const *>(hash.data()), hashLen};
}

std::string Utils::sha256(const std::string &str)
{
    std::string hash;
    hash.resize(SHA256_DIGEST_LENGTH);
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final((unsigned char *)hash.data(), &sha256);
    return hash;
}

std::string Utils::sha1(const std::string &str)
{
    std::string hash;
    hash.resize(SHA_DIGEST_LENGTH);
    SHA_CTX sha1;
    SHA1_Init(&sha1);
    SHA1_Update(&sha1, str.c_str(), str.size());
    SHA1_Final((unsigned char *)hash.data(), &sha1);
    return hash;
}

std::string Utils::md5(const std::string & msg, size_t out_len) {
    std::string res;
    res.resize(16);
    MD5((const uint8_t*)msg.c_str(), msg.size(), (uint8_t*)res.data());
    std::string res2;
    
    bin_to_hex_str(res, res2);
    if (out_len == 16) {
        return res2.substr(8, 16);
    }
    return res2;
}

bool Utils::parse_url(const std::string &url, std::string &protocol, std::string &domain, uint16_t &port, std::string &path, std::unordered_map<std::string, std::string> &params)
{
    // http://test.publsh.com:80/api/on_publish?sign=123456
    // https://test.publsh.com/live/app/stream.flv?sign=123456
    std::vector<std::string> vs;
    boost::split(vs, url, boost::is_any_of("?"));
    if (vs.size() <= 0)
    {
        return false;
    }

    std::vector<std::string> vs_url;
    boost::split(vs_url, vs[0], boost::is_any_of("/"));
    if (vs_url.size() <= 0)
    {
        return false;
    }
    // 获取协议名称(http/https)
    auto &tmp1 = vs_url[0];
    auto pos = tmp1.find_last_of(":");
    if (pos == std::string::npos)
    {
        return false;
    }
    protocol = tmp1.substr(0, pos);
    // 空字符
    if (vs_url.size() <= 1)
    {
        return false; //
    }
    if (vs_url[1] != "")
    {
        return false;
    }
    // 域名
    if (vs_url.size() <= 2)
    {
        return false;
    }
    auto comma_pos = vs_url[2].find_last_of(":");
    if (comma_pos == std::string::npos)
    {
        domain = vs_url[2];
        port = 80;
    }
    else
    {
        std::vector<std::string> vs_domain;
        boost::split(vs_domain, vs_url[2], boost::is_any_of(":"));
        domain = vs_domain[0];
        port = std::atoi(vs_domain[1].c_str());
    }

    // path
    if (vs_url.size() >= 4)
    { // todo：这里性能有点影响，但不大
        std::vector<std::string> vpath;
        for (size_t i = 3; i < vs_url.size(); i++)
        {
            vpath.push_back(vs_url[i]);
        }
        path = "/" + boost::join(vpath, "/");
    }
    // param
    if (vs.size() <= 1)
    {
        return true;
    }
    std::vector<std::string> vs_param;
    boost::split(vs_param, vs[1], boost::is_any_of("&"));
    if (vs_param.size() <= 0)
    {
        return true;
    }

    for(auto & s : vs_param) {
        auto equ_pos = s.find("=");
        if (equ_pos == std::string::npos) {
            continue;
        }

        std::string name = s.substr(0, equ_pos);
        std::string value = s.substr(equ_pos + 1);
        params[name] = value;
    }
    return true;
}

/*-----------------------------------------
局域网IP地址范围
A类：10.0.0.0-10.255.255.255
B类：172.16.0.0-172.31.255.255
C类：192.168.0.0-192.168.255.255
-------------------------------------------*/
bool Utils::is_lan(const std::string & sip)
{
    auto ip_addr = boost::asio::ip::make_address(sip);
    if (ip_addr.is_v4()) {
        std::istringstream st(sip);
        int ip[2];
        for (int i = 0; i < 2; i++)
        {
            std::string temp;
            std::getline(st, temp, '.');
            std::istringstream a(temp);
            a >> ip[i];
        }

        if ((ip[0] == 10) || (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) || (ip[0] == 192 && ip[1] == 168)) {
            return true;
        } else {
            return false;
        }
    } else if (ip_addr.is_v6()) {
        auto addr_v6 = ip_addr.to_v6();
        if (!addr_v6.is_multicast() && !addr_v6.is_loopback() && !addr_v6.is_link_local())
        {
            return false;
        }
        return true;
    }
    return true;
}

bool Utils::is_wan(const std::string & sip) {
    if (sip == "127.0.0.1") {
        return false;
    }

    if (is_lan(sip)) {
        return false;
    }

    return true;
}

bool Utils::encode_base64( const std::string & input, std::string & output )
{
	typedef boost::archive::iterators::base64_from_binary<boost::archive::iterators::transform_width<std::string::const_iterator, 6, 8>> Base64EncodeIterator;
	std::stringstream result;
	try {
		std::copy( Base64EncodeIterator( input.begin() ), Base64EncodeIterator( input.end() ), std::ostream_iterator<char>( result ) );
	} catch ( ... ) {
		return false;
	}
	size_t equal_count = (3 - input.length() % 3) % 3;
	for ( size_t i = 0; i < equal_count; i++ )
	{
		result.put( '=' );
	}
	output = result.str();
	return output.empty() == false;
}
 
bool Utils::decode_base64( const std::string & input, std::string & output )
{
	typedef boost::archive::iterators::transform_width<boost::archive::iterators::binary_from_base64<std::string::const_iterator>, 8, 6> Base64DecodeIterator;
	std::stringstream result;
	try {
		std::copy( Base64DecodeIterator( input.begin() ), Base64DecodeIterator( input.end() ), std::ostream_iterator<char>( result ) );
	} catch ( ... ) {
		return false;
	}
	output = result.str();
	return output.empty() == false;
}

bool Utils::bin_to_hex_str(const std::string & input, std::string & output, bool upper) 
{
    output.resize(input.size()*2);
    for (size_t i = 0; i < input.size(); i++) {
        uint8_t d = input[i];
        uint8_t t = (d>>4)&0x0f;
        if (t < 10) {
            t += '0';
        } else {
            t += (upper?'A':'a') - 10;
        }
        output[i*2] = t;

        t = d&0x0f;
        if (t < 10) {
            t += '0';
        } else {
            t += (upper?'A':'a') - 10;
        }
        output[i*2+1] = t;
    }
    return true;
}

bool Utils::hex_str_to_bin(const std::string & input, std::string & output) {
    for (int i = 0; i+1 < (int)input.size(); i += 2) {
        uint8_t d = 0;
        char c0 = input[i];
        char c1 = input[i+1];
        if (c0 >= '0' && c0 <= '9') {
            d = c0 - '0';
        } else if (c0 >= 'a' && c0 <= 'f') {
            d = c0 - 'a' + 10;
        } else if (c0 >= 'A' && c0 <= 'F') {
            d = c0 - 'A' + 10;
        }

        d = d << 4;
        if (c1 >= '0' && c1 <= '9') {
            d |= c1 - '0';
        } else if (c1 >= 'a' && c1 <= 'f') {
            d |= c1 - 'a' + 10;
        } else if (c1 >= 'A' && c1 <= 'F') {
            d |= c1 - 'A' + 10;
        }
        output.append(1, (char)d);
    }
    return true;
}

int64_t Utils::get_current_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

std::string Utils::get_utc_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_struct;
    gmtime_r(&now_time_t, &tm_struct); // 转换为UTC时间的tm结构体
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_struct);
    return std::string(buffer);
}

std::string Utils::get_utc_time_with_millis() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto now_time_t = system_clock::to_time_t(now);

    std::tm tm_struct;
    gmtime_r(&now_time_t, &tm_struct);

    // 计算毫秒部分
    auto duration_since_epoch = now.time_since_epoch();
    auto millis = duration_cast<milliseconds>(duration_since_epoch).count() % 1000;
    // 格式化时间（不含毫秒）
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%dT%H:%M:%S", &tm_struct);
    // 拼接毫秒和 'Z'
    char final_buffer[80];
    snprintf(final_buffer, sizeof(final_buffer), "%s.%03lldZ", time_buffer, static_cast<long long>(millis));
    return std::string(final_buffer);
}

const unsigned long EPOCH = 2208988800UL; // delta between epoch time and ntp time
const double NTP_SCALE_FRAC = 4294967295.0; // maximum value of the ntp fractional part
int64_t Utils::get_ntp_time() {
    struct timeval tv;
    uint64_t ntp_time;
    uint64_t tv_ntp;
    double tv_usecs;

    gettimeofday(&tv, NULL);
    tv_ntp = tv.tv_sec + EPOCH;

    // convert tv_usec to a fraction of a second
    // next, we multiply this fraction times the NTP_SCALE_FRAC, which represents
    // the maximum value of the fraction until it rolls over to one. Thus,
    // .05 seconds is represented in NTP as (.05 * NTP_SCALE_FRAC)
    tv_usecs = (tv.tv_usec * 1e-6) * NTP_SCALE_FRAC;

    // next we take the tv_ntp seconds value and shift it 32 bits to the left. This puts the 
    // seconds in the proper location for NTP time stamps. I recognize this method has an 
    // overflow hazard if used after around the year 2106
    // Next we do a bitwise OR with the tv_usecs cast as a uin32_t, dropping the fractional
    // part
    ntp_time = ((tv_ntp << 32) | (uint32_t)tv_usecs);
    return ntp_time;
}

std::string Utils::get_bin_path() {
    char dir[4096] = {0};
    ssize_t n = readlink("/proc/self/exe", dir, 4096);
    if (n <= 0) {
        static std::string empty_str;
        return empty_str;
    }
    
    std::filesystem::path path(std::string(dir, n));
    auto p = path.remove_filename();
    return p.string();
}

bool Utils::aes_encrypt(const std::string &plaintext, const std::string &key, std::string &ciphertext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }
 
    bool ret = true;
    const EVP_CIPHER *cipher = EVP_aes_256_cbc();
 
    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, (const unsigned char*)key.c_str(), nullptr) != 1) {
        ret = false;
    }
 
    int len = plaintext.length();
    unsigned char out[len + EVP_MAX_BLOCK_LENGTH];
    int outlen = 0;
 
    if (ret && EVP_EncryptUpdate(ctx, out, &outlen, (const unsigned char*)plaintext.c_str(), len) != 1) {
        ret = false;
    }
 
    if (ret && EVP_EncryptFinal_ex(ctx, out + outlen, &len) != 1) {
        ret = false;
    }
 
    EVP_CIPHER_CTX_free(ctx);
 
    if (ret) {
        ciphertext.assign(reinterpret_cast<const char*>(out), outlen + len);
    }
 
    return ret;
}
 
bool Utils::aes_decrypt(const std::string &ciphertext, const std::string &key, std::string &plaintext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }
 
    bool ret = true;
    const EVP_CIPHER *cipher = EVP_aes_256_cbc();
 
    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, (const unsigned char*)key.c_str(), nullptr) != 1) {
        ret = false;
    }
 
    int len = ciphertext.length();
    unsigned char out[len];
    int outlen = 0;
 
    if (ret && EVP_DecryptUpdate(ctx, out, &outlen, (const unsigned char*)ciphertext.c_str(), len) != 1) {
        ret = false;
    }
 
    if (ret && EVP_DecryptFinal_ex(ctx, out + outlen, &len) != 1) {
        ret = false;
    }
 
    EVP_CIPHER_CTX_free(ctx);
 
    if (ret) {
        plaintext.assign(reinterpret_cast<const char*>(out), outlen + len);
    }
 
    return ret;
}

bool Utils::is_ip_address(const std::string & str) {
    std::regex ip_regex(
        R"(^(?:[0-9]{1,3}.){3}[0-9]{1,3}$)"
    );
    return std::regex_match(str, ip_regex);
}