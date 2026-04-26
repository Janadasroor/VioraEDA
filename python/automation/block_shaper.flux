# Smart Signal Block Shaper Automation
# Use this in the Script Editor Tab

let count = erc_get_component_count() in
let i = 0 in

while i < count do {
    let type = erc_get_type(i) in
    if type == "SmartSignalBlock" then {
        print("Shaping...");
        erc_set_block_pins(i, "IN1,IN2,RESET", "OUT");
    } else {
        0.0;
    };
    
    i = i + 1;
}

print("Shaping complete.")
