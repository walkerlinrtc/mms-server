#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <math.h>
#include <openssl/md5.h>
#include <set>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp> 
namespace mms {

//定义哈希反函数对象
template <typename K, typename V>
class Hash;
//虚拟节点
template <typename N>
class VNode;
//哈希仿函数对象,v为std::string, 映射为uint32_t
template <>
class Hash<std::string, unsigned long> {
public:
    unsigned long operator()(const std::string& key) const
    {
        u_char digest[16];
        MD5_CTX md5;

        MD5_Init(&md5);
        MD5_Update(&md5, key.c_str(), key.size());
        MD5_Final(digest, &md5);

        unsigned int hash = 0;

        for (int i = 0; i < 4; ++i) {
            hash += ((long)(digest[i * 4 + 3] & 0xFF) << 24)
                | ((long)(digest[i * 4 + 2] & 0xFF) << 16)
                | ((long)(digest[i * 4 + 1] & 0xFF) << 8)
                | ((long)(digest[i * 4 + 0] & 0xFF));
        }
        return hash;
    }
};
//哈希环算法对象，内部可调用不同哈希算法将node映射到uint32_t
template <typename V>
class HashRingAlgorithm {
public:
    HashRingAlgorithm() { }
    virtual ~HashRingAlgorithm() { }

public:
    unsigned long hash(const V& v) const
    {
        Hash<V, unsigned long> h;
        return h(v);
    }
};

/*
 * 虚拟节点,模板参数N为server真实节点
*/
template <typename N>
class VNode {
public:
    VNode() {}; //just make compiler happy
    VNode(const N& n, uint32_t idx)
    {
        key_ = n.get_key() + "/" + boost::lexical_cast<std::string>(idx); 
        rnode_ = n;
    };

    virtual ~VNode() {

    };

    const std::string & get_key() const
    {
        return key_;
    }

    const N & getRNode() const
    {
        return rnode_;
    }

private:
    std::string key_;
    N rnode_; //真实节点
};

/*
 * 一致性哈希环对象，顶层对象,N为Server类，K为映射key如std::string
*/
template <typename N, typename K>
class ConsitentHashRing {
public:
    ConsitentHashRing(uint32_t vn_count = 1000)
    {
        vn_count_ = vn_count;
    }

    virtual ~ConsitentHashRing()
    {
        vnodes_map_.clear();
        real_nodes_.clear();
    }

public:
    void add_node(const N& n)
    {
        for (uint32_t i = 0; i < vn_count_; i++) {
            VNode<N> vnode(n, i);
            unsigned long hash = hash_algorithm_.hash(vnode.get_key());
            vnodes_map_[hash] = vnode;
        }
        real_nodes_.push_back(n);
    }

    void add_node(const N & n, uint32_t vn_count) {
        for (uint32_t i = 0; i < vn_count; i++) {
            VNode<N> vnode(n, i);
            unsigned long hash = hash_algorithm_.hash(vnode.get_key());
            vnodes_map_[hash] = vnode;
        }
        real_nodes_.push_back(n);
    }

    int get_node(const K& v, N & n) const
    {
        unsigned long hash = hash_algorithm_.hash(v);
        auto it = vnodes_map_.lower_bound(hash);
        if (it == vnodes_map_.end()) {
            if (vnodes_map_.begin() == vnodes_map_.end()) {
                return -1;
            }
            n = vnodes_map_.begin()->second.getRNode();
            return 0;
        }
        n = it->second.getRNode();
        return 0;
    }

    int get_node(const K& v, N & n, const std::function<bool(const N & n)> & exclude_fun) const
    {
        unsigned long hash = hash_algorithm_.hash(v);
        auto it_start = vnodes_map_.lower_bound(hash);
        auto it = it_start;
        if (vnodes_map_.begin() == vnodes_map_.end()) {
            return -1;
        }

        while(it != vnodes_map_.end()) {
            auto rn = it->second.getRNode();
            if (exclude_fun(rn)) {
                it++;
            } else {
                n = it->second.getRNode();
                return 0;
            }
        }

        if (it == vnodes_map_.end()) {
            it = vnodes_map_.begin();
            while(it != it_start) {
                auto rn = it->second.getRNode();
                if (exclude_fun(rn)) {
                    it++;
                } else {
                    n = it->second.getRNode();
                    return 0;
                }
            }
        }

        return -2;
    }

    int remove_node(const N& n)
    {
        for (auto it = vnodes_map_.begin(); it != vnodes_map_.end();) {
            if (it->second.getRNode() == n) {
                vnodes_map_.erase(it++);
            } else {
                it++;
            }
        }

        for (auto it = real_nodes_.begin(); it != real_nodes_.end(); it++) {
            if (*it == n) {
                real_nodes_.erase(it);
                break;
            }
        }
        return 0;
    }

    const std::vector<N>& get_real_nodes()
    {
        return real_nodes_;
    }

    void clear()
    {
        vnodes_map_.clear();
        real_nodes_.clear();
    }

private:
    uint32_t vn_count_;
    std::map<unsigned long, VNode<N>> vnodes_map_;
    std::vector<N> real_nodes_;
    HashRingAlgorithm<K> hash_algorithm_;
};
};