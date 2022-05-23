#pragma once

namespace minisandbox::empowerment {
    void drop_privileges();
    bool set_capabilities(const char*);
};
