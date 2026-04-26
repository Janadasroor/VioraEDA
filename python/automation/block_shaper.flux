# Smart Signal Block Shaper Automation (Clean)
# Use this in the Script Editor Tab

count = erc_get_component_count()
i = 0

while i < count do {
    type = erc_get_type(i)
    if type == "SmartSignalBlock" then {
        print("Shaping block: " + erc_get_ref(i))
        erc_set_block_pins(i, "IN1,IN2,RESET", "OUT")
    } else 0.0
    
    i = i + 1
}

print("Shaping complete.")
