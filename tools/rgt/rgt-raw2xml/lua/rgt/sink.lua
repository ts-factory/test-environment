---
-- Report generation tool - sink module.
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
local co            = require("co")
local rgt           = {}
rgt.node            = {}
rgt.node.root       = require("rgt.node.root")
rgt.node.package    = require("rgt.node.package")
rgt.node.session    = require("rgt.node.session")
rgt.node.test       = require("rgt.node.test")
rgt.sink            = oo.class({
                                manager = nil,  --- The output chunk manager
                                chunk   = nil,  --- The output chunk
                                map     = nil,  --- ID->node map
                               })

function rgt.sink:__init(max_mem)
    return oo.rawnew(self, {manager = co.manager(max_mem), map = {}})
end

function rgt.sink:take_file(file)
    self.chunk = co.xml_chunk(self.manager, file, 0)
    self.manager:set_first(self.chunk)
end

function rgt.sink:start()
    self.map[0] = rgt.node.root({}):take_chunk(self.chunk):start()
    self.chunk = nil
end

function rgt.sink:put(msg)
    local prm
    local parent, node

    -- If it is not a control message
    if not msg:is_ctl() then
        -- log the message to its node
        node = self.map[msg.id]
        if node == nil then
            error(("Node ID %u not found"):format(msg.id))
        end
        node:log(msg)
        return
    end

    -- parse control message text
    prm = msg:extr_ctl()

    -- lookup parent node
    parent = self.map[prm.parent_id]
    if parent == nil then
        error(("Node ID %u not found"):format(prm.parent_id))
    end

    -- If it is a node opening
    if prm.event == "package" or
       prm.event == "session" or
       prm.event == "test" then
        -- create new node
        node = rgt.node[prm.event]({
                start_ts    = msg.ts,
                name        = prm.name,
                objective   = prm.objective,
                tin         = prm.tin,
                page        = prm.page,
                authors     = prm.authors,
                args        = prm.args})

        -- add to the map
        self.map[prm.id] = node

        -- add to the parent's children list
        parent:add_child(node)

        -- start node output
        node:start()
    else
        -- lookup node
        node = self.map[prm.id]
        if node == nil then
            error(("Node ID %u not found"):format(prm.id))
        end

        -- finish the node with the result
        node:finish(msg.ts, prm.event, prm.err)

        -- remove the node from the parent
        parent:del_child(node)

        -- remove the node from the map
        self.map[prm.id] = nil
    end
end

function rgt.sink:finish()
    -- Finish the root node and take its chunk
    self.chunk = self.map[0]:finish():yield_chunk()
    -- Remove the root node
    self.map[0] = nil
end

function rgt.sink:yield_file()
    local file, size

    -- Finish the chunk and retrieve its storage and size
    file, size = self.chunk:finish():yield()
    -- Remove the chunk
    self.chunk = nil

    return file, size
end

return rgt.sink
