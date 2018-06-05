#include "taggeduser.hpp"

#include <tuple>

namespace chatterino {
namespace controllers {
namespace taggedusers {

TaggedUser::TaggedUser(ProviderId _provider, const QString &_name, const QString &_id)
    : provider(_provider)
    , name(_name)
    , id(_id)
{
}

bool TaggedUser::operator<(const TaggedUser &other) const
{
    return std::tie(this->provider, this->name, this->id) <
           std::tie(other.provider, other.name, other.id);
}

}  // namespace taggedusers
}  // namespace controllers
}  // namespace chatterino
