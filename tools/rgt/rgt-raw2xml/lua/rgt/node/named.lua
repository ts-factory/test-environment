---
-- Report generation tool - named node interface.
--
-- Copyright (C) 2010 Test Environment authors (see file AUTHORS in the
-- root directory of the distribution).
-- 
-- Test Environment is free software; you can redistribute it and/or
-- modify it under the terms of the GNU General Public License as
-- published by the Free Software Foundation; either version 2 of
-- the License, or (at your option) any later version.
-- 
-- Test Environment is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
-- 
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston,
-- MA  02111-1307  USA
-- 
-- @author Nikolai Kondrashov <Nikolai.Kondrashov@oktetlabs.ru>
-- 
-- @release $Id$
--

local oo        = require("loop.base")
local rgt       = {}
rgt.node        = {}
rgt.node.named  = oo.class({
                            name        = nil,  --- Name string
                            objective   = nil,  --- Objective string
                            authors     = nil,  --- Author e-mail array
                           })

function rgt.node.named:__init(inst)
    assert(type(inst) == "table")
    assert(type(inst.name) == "string")
    assert(type(inst.objective) == "string")
    assert(type(inst.authors) == "table")

    return oo.rawnew(self, inst)
end

return rgt.node.named
