-------------------------------------------------------------------------------
-- Send some text to the owner of this entity
-------------------------------------------------------------------------------
tt.owner_say("Hello World!")

-------------------------------------------------------------------------------
-- Send text when clicked on
-------------------------------------------------------------------------------
-- Make sure to check the box for making this gadget usable
local me = entity.me()
function use(t)
  me:say("Hello World!")
end
me:set_callback(use, "use")

-------------------------------------------------------------------------------
-- Echo received private messages back to the user
-------------------------------------------------------------------------------
function private_message(t)
  entity.me():tell(t.id, t.text)
end

entity.me():set_callback(private_message, "private_message")

-------------------------------------------------------------------------------
-- Run code when the script is stopped
-------------------------------------------------------------------------------
-- Note that this might not be 100% guaranteed to run
function shutdown()
  tt.owner_say("Bye!")
end

tt.set_callback(shutdown, "shutdown")

-------------------------------------------------------------------------------
-- Buttons that users can click on in the chat
-------------------------------------------------------------------------------
function bot_message_button(t)
  entity.me():tell(t.id, "You clicked on my button! "..t.text)
end

tt.owner_say("[bot-message-button]aaaaa[/bot-message-button]")
entity.me():set_callback(bot_message_button, "bot_message_button")

-------------------------------------------------------------------------------
-- Persistent storage
-------------------------------------------------------------------------------
storage.reset()
storage.save("key1", "value1")
storage.save("key2", "value2")
tt.owner_say(storage.load("key1"))
tt.owner_say(storage.load("key2"))

-------------------------------------------------------------------------------
-- Wavy text that moves across the screen
-------------------------------------------------------------------------------
local a = bitmap_4x2.new()
a.color = 2
a.invert = false
local x = 0
local y = 0
local timer = 0
while true do
  a:clear(false)
  a:text(x,    y+math.sin(timer)*5,   true, "T")
  a:text(x+4,  y+math.sin(timer+1)*5, true, "e")
  a:text(x+8,  y+math.sin(timer+2)*5, true, "x")
  a:text(x+12, y+math.sin(timer+3)*5, true, "t")
  x += 1
  timer += 1
  if x == 32 then
    x = 0
    y = math.random(0, 22)
    a.color = math.random(0, 16)
  end
  entity.me():set_mini_tilemap(a)
  tt.sleep_next(500)
end

-------------------------------------------------------------------------------
-- Drawing test
-------------------------------------------------------------------------------
local a = bitmap_4x2.new()
a.color = 5
a.invert = false
a:clear(false)

a:text(16, 0, true, "Text")

a:rect_fill(0, 0, 10, 10, true)
for i=0,31 do
  a:put(0, i, true)
  a:put(i, i, true)
end
a:rect(25, 10, 5, 5, true)

b = bitmap_sprite.new{0b00111100, 0b01000010, 0b10100101, 0b10000001, 0b10100101, 0b10011001, 0b01000010, 0b00111100}
a:draw_sprite(b, 4, 20, true, false, false)

local e = entity.me()
e:set_mini_tilemap(a)

-------------------------------------------------------------------------------
-- Show a bunch of lines of text
-------------------------------------------------------------------------------
local a = bitmap_4x2.new()
a.map_width = 16
a.map_height = 24
a.invert = false
a.color = 5
a:clear(false)
--                      ---------------
a:text(2, 3+7*0, true, "This is the")
a:text(2, 3+7*1, true, "largest amount")
a:text(2, 3+7*2, true, "of text you can")
a:text(2, 3+7*3, true, "fit on one of")
a:text(2, 3+7*4, true, "these screens.")
a:text(2, 3+7*5, true, "Pretty cool!")
a:rect(0, 0, 16*4, 24*2)

entity.me():set_mini_tilemap(a)

-------------------------------------------------------------------------------
-- Capture someone's keys and move an entity around
-------------------------------------------------------------------------------
local me = entity.me()
function use(t)
  if me:have_controls_for(t.id) then
    me:release_controls(t.id)
  else
    me:take_controls(t.id, "move", false, true)
  end
end
keys = {}
function got_key(t)
  keys[t.key] = t.down
end

me:set_callback(use, "use")
me:set_callback(got_key, "key_press")

local a = bitmap_4x2.new()
a.color = 2
a.invert = false
local player_x = 0
local player_y = 0
local timer = 0
while true do
  a:clear(false)
  if keys["move-w"] then player_x -= 1 end
  if keys["move-e"] then player_x += 1 end
  if keys["move-n"] then player_y -= 1 end
  if keys["move-s"] then player_y += 1 end
  a:text(player_x, player_y, true, "@")
  entity.me():set_mini_tilemap(a)
  tt.sleep_next(100)
end

