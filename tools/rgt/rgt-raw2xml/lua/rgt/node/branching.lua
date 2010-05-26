---
-- Report generation tool - branching node type.
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

local oo            = require("loop.simple")
local rgt           = {}
rgt.node            = {}
rgt.node.internal   = require("rgt.node.internal")
rgt.node.branch     = require("rgt.node.branch")
rgt.node.branching  = oo.class({
                                branches    = nil   --- branch array
                               }, rgt.node.internal)

function rgt.node.branching:__init(inst)
    assert(type(inst) == "table")

    rgt.node.internal:__init(inst)

    inst.branches = {}

    return oo.rawnew(self, inst)
end


function rgt.node.branching:add_child(node)
    local branch

    assert(oo.instanceof(node, rgt.node.basic))

    -- Lookup first child-free branch
    for i, b in ipairs(self.branches) do
        if b:get_child() == nil then
            branch = b
            break
        end
    end

    -- If a vacant branch is not found
    if branch == nil then
        -- Create new branch
        branch = rgt.node.branch({depth = self.depth})
        table.insert(self.branches, branch)
        branch:start(self.tail_chunk:fork_prev())
    end

    -- Add the child to the branch
    branch:add_child(node)

    -- Make the branch be our child, if there was none before
    if self.child == nil then
        self.child = branch
    end
end

function rgt.node.branching:del_child(node)
    local branch

    assert(oo.instanceof(node, rgt.node.basic))

    -- Lookup the branch holding the child we're deleting
    for i, b in ipairs(self.branches) do
        if b:get_child() == node then
            branch = b
            break
        end
    end

    assert(branch ~= nil)

    -- Delete the child from the branch
    branch:del_child(node)

    -- If this branch is not our child
    if branch ~= self.child then
        return
    end

    -- Lookup the first branch with a child
    branch = nil
    for i, b in ipairs(self.branches) do
        if b:get_child() ~= nil then
            branch = b
            break
        end
    end

    -- Make it (if any) our child
    self.child = branch
end


return rgt.node.branching
