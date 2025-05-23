push_textdomain("tribes")

local dirname = path.dirname (__file__)

wl.Descriptions():new_market_type {
   name = "empire_market",
   -- TRANSLATORS: This is a building name used in lists of buildings
   descname = pgettext("empire_building", "Market"),
   animation_directory = dirname,
   icon = dirname .. "menu.png",
   size = "big",

   buildcost = {
      log = 3,
      planks = 2,
      granite = 2,
      -- TODO(tothxa): make this marble=2 and column=2 if column is made to only cost 1 marble
      marble = 3,
      marble_column = 1,
      cloth = 2,
   },
   return_on_dismantle = {
      log = 1,
      planks = 1,
      granite = 1,
      marble = 2,
      cloth = 1,
   },

   local_carrier = "empire_carrier",
   trade_carrier = "empire_donkey",

   animation_directory = dirname,
   animations = {
      idle = {
         hotspot = { 104, 97 }
      },
   },
   spritesheets = {
      build = {
         frames = 2,
         columns = 2,
         rows = 1,
         hotspot = { 104, 97 }
      }
   },

   aihints = {
      prohibited_till = 1000,
      forced_after = 1500,
   },
}

pop_textdomain()
