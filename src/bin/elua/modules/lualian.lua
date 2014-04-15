-- Elua lualian module

local cutil  = require("cutil")
local util   = require("util")
local log    = require("eina.log")
local eolian = require("eolian")

local M = {}

local dom

cutil.init_module(function()
    dom = log.Domain("lualian")
    if not dom:is_valid() then
        log.err("Could not register log domain: lualian")
        error("Could not register log domain: lualian")
    end
end, function()
    dom:unregister()
    dom = nil
end)

local Node = util.Object:clone {
    generate = function(self, s)
    end,

    gen_children = function(self, s)
        for i, v in ipairs(self.children) do
            v:generate(s)
        end
    end
}

local Method = Node:clone {
}

local Property = Node:clone {
}

local Constructor = Node:clone {
}

local Destructor = Node:clone {
}

local Mixin = Node:clone {
    __ctor = function(self, cname, ch)
        self.cname    = cname
        self.children = ch
    end,

    generate = function(self, s)
        dom:log(log.level.INFO, "  Generating for interface/mixin: "
            .. self.cname)
        s:write(("M.%s = {\n"):format(self.cname))

        self:gen_children(s)

        s:write("\n}\n")
    end
}

local Class = Node:clone {
    __ctor = function(self, cname, parent, mixins, ch)
        self.cname      = cname
        self.parent     = parent
        self.interfaces = interfaces
        self.mixins     = mixins
        self.children   = ch
    end,

    generate = function(self, s)
        dom:log(log.level.INFO, "  Generating for class: " .. self.cname)
        s:write(([[
local Parent = eo_get("%s")
M.%s = Parent:clone {
]]):format(self.parent, self.cname))

        self:gen_children(s)

        s:write("\n}\n")

        for i, v in ipairs(self.mixins) do
            s:write("\nM.%s:mixin(eo_get(\"%s\"))\n", self.cname, v)
        end
    end
}

local File = Node:clone {
    __ctor = function(self, fname, cname, ch)
        self.fname = fname
        self.cname = cname
        self.children = ch
    end,

    generate = function(self, s)
        dom:log(log.level.INFO, "Generating for file: " .. self.fname)
        dom:log(log.level.INFO, "  Class            : " .. self.cname)
        s:write(([[
-- EFL LuaJIT bindings: %s (class %s)
-- For use with Elua; automatically generated, do not modify

local M = {}

]]):format(self.fname, self.cname))

        self:gen_children(s)

        s:write([[

return M
]])
    end
}

local gen_contents = function(classn)
    return {}
end

local gen_mixin = function(classn)
    return Mixin(classn, gen_contents(classn))
end

local gen_class = function(classn)
    local inherits = eolian.class_inherits_list_get(classn)
    local parent
    local mixins   = {}
    local ct = eolian.class_type
    for i, v in ipairs(inherits) do
        local tp = eolian.class_type_get(v)
        if tp == ct.REGULAR or tp == ct.ABSTRACT then
            if parent then
                error(classn .. ": more than 1 parent!")
            end
            parent = v
        elseif tp == ct.MIXIN or tp == ct.INTERFACE then
            mixins[#mixins + 1] = v
        else
            error(classn .. ": unknown inherit")
        end
    end
    return Class(classn, parent, mixins, gen_contents(classn))
end

M.generate = function(fname, fstream)
    if not eolian.eo_file_parse(fname) then
        error("Failed parsing file: " .. fname)
    end
    local classn = eolian.class_find_by_file(fname)
    local tp = eolian.class_type_get(classn)
    local ct = eolian.class_type
    local cl
    if tp == ct.MIXIN or tp == ct.INTERFACE then
        cl = gen_mixin(classn)
    elseif tp == ct.REGULAR or tp == ct.ABSTRACT then
        cl = gen_class(classn)
    else
        error(classn .. ": unknown type")
    end
    File(fname, classn, { cl }):generate(fstream)
end

return M