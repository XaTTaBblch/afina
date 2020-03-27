#include "SimpleLRU.h"
#include <memory>

namespace std {
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}

namespace Afina {
namespace Backend {

void SimpleLRU::DeleteNode(lru_node &tmp) {
    auto next = std::move(tmp.next);
    auto prev = tmp.prev;

    if(next) {
        next->prev = prev;
    } else {
        _lru_tail = prev;
    }

    if(prev) {
        prev->next = std::move(next);
    } else {
        _lru_head = std::move(next);
    }

    _cur_size -= tmp.key.size() + tmp.value.size();
}

void SimpleLRU::MoveNodeToTail(lru_node &tmp) {
    if(!tmp.next) {
        return;
    }
    auto next = std::move(tmp.next);
    auto prev = tmp.prev;

    next->prev = prev;

    if(prev) {
        _lru_tail->next = std::move(prev->next);
        prev->next = std::move(next);
    } else {
        _lru_tail->next = std::move(_lru_head);
        _lru_head = std::move(next);
    }

    tmp.prev = _lru_tail;
    _lru_tail = &tmp;
}

void SimpleLRU::InsertNewNode(const std::string &key, const std::string &value) {
    while(_cur_size + key.size() + value.size() > _max_size) {
        _lru_index.erase(_lru_head->key);
        DeleteNode(*_lru_head.get());
    }

    _cur_size += key.size() + value.size();

    if(!_lru_head) {
        _lru_head = std::make_unique<lru_node>(lru_node{ key, value, nullptr, nullptr });
        _lru_tail = _lru_head.get();

        _lru_index.insert({std::cref(_lru_tail->key), std::ref(*_lru_tail)});
        return;
    }
    _lru_tail->next = std::make_unique<lru_node>(lru_node{ key, value, _lru_tail, nullptr });
    _lru_tail = _lru_tail->next.get();

    _lru_index.insert({std::cref(_lru_tail->key), std::ref(*_lru_tail)});
}

void SimpleLRU::ChangeNode(lru_node &tmp, const std::string &value) {
    MoveNodeToTail(tmp);

    while(_cur_size - tmp.value.size() + value.size() > _max_size) {
        _lru_index.erase(_lru_head->key);
        DeleteNode(*_lru_head.get());
    }

    _cur_size -= tmp.value.size();
    _cur_size += value.size();
    tmp.value = value;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    if(key.size() + value.size() > _max_size)
        return false;

    auto it = _lru_index.find(key);
    if(it != _lru_index.end()) {
        ChangeNode(it->second.get(), value);
        return true;
    }

    InsertNewNode(key, value);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if(key.size() + value.size() > _max_size)
        return false;

    auto it = _lru_index.find(key);
    if(it != _lru_index.end()) {
        return false;
    }

    InsertNewNode(key, value);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if(key.size() + value.size() > _max_size)
        return false;
    auto it = _lru_index.find(key);
    if(it == _lru_index.end()) {
        return false;
    }

    ChangeNode(it->second.get(), value);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto it = _lru_index.find(key);
    if(it == _lru_index.end()) {
        return false;
    }

    lru_node &tmp = it->second.get();
    _lru_index.erase(it);

    DeleteNode(tmp);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto it = _lru_index.find(key);
    if(it == _lru_index.end()) {
        return false;
    }

    lru_node &tmp = it->second.get();
    value = tmp.value;

    MoveNodeToTail(tmp);

    return true;
}

} // namespace Backend
} // namespace Afina
