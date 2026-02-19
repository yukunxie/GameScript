#include "gs/binding.hpp"

#include <stdexcept>

namespace gs {

void HostRegistry::bind(const std::string& name, HostFunction fn) {
    funcs_[name] = std::move(fn);
}

bool HostRegistry::has(const std::string& name) const {
    return funcs_.contains(name);
}

Value HostRegistry::invoke(const std::string& name, HostContext& context, const std::vector<Value>& args) const {
    auto it = funcs_.find(name);
    if (it == funcs_.end()) {
        throw std::runtime_error("Host function not found: " + name);
    }
    return it->second(context, args);
}

} // namespace gs
