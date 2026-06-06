#pragma once

#include "Types.h"

#include <string>
#include <vector>

namespace tcx::ios {

struct PickedContact {
    std::string identifier;
    std::string givenName;
    std::string familyName;
    std::vector<std::string> phoneNumbers;
    std::vector<std::string> emailAddresses;
};

class ContactsUI {
public:
    void pickContact(Completion<PickedContact> done);
};

ContactsUI& contactsUI();

} // namespace tcx::ios
