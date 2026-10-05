#pragma once
//=============================================================================
//
// 任意クラスに対してシングルトン機能を提供するテンプレート基底 [SingletonBase.h]
// Author : 
// 継承先に静的唯一インスタンス生成を提供し、コピー・代入を禁止することで、
// グローバルで一貫性のあるリソース管理を可能にする
//
//=============================================================================
#include <iostream>

template <typename T>
class SingletonBase
{
public:

    virtual ~SingletonBase() {
    }

    SingletonBase(const SingletonBase&) = delete;

    SingletonBase& operator=(const SingletonBase&) = delete;

    static T& get_instance() {
        static T instance;

        return instance;
    }

protected:

    
    SingletonBase() {
    }
};
