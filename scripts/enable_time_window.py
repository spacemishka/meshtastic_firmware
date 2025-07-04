Import("env")

def enable_time_window(source, target, env):
    """Enable time window feature and set default configuration"""
    
    # Get config object
    config = env.GetProjectConfig()
    
    # Enable time window feature
    if not config.has_section("features"):
        config.add_section("features")
    config.set("features", "time_window", "1")
    
    # Set default time window options if not present
    if not config.has_section("time_window_options"):
        config.add_section("time_window_options")
        defaults = {
            "enabled": "true",
            "start_hour": "21",
            "start_minute": "0",
            "end_hour": "23",
            "end_minute": "0",
            "mode": "QUEUE_PACKETS",
            "queue_size": "32",
            "packet_expiry": "3600"
        }
        for key, value in defaults.items():
            if not config.has_option("time_window_options", key):
                config.set("time_window_options", key, value)
    
    # Update build flags
    build_flags = env.GetProjectOption("build_flags", [])
    if isinstance(build_flags, str):
        build_flags = [build_flags]
    
    # Add time window specific flags
    time_window_flags = [
        "-D HAS_TIME_WINDOW",
        "-D TIME_WINDOW_QUEUE_SIZE={}".format(
            config.get("time_window_options", "queue_size")),
        "-D TIME_WINDOW_DEFAULT_MODE={}".format(
            1 if config.get("time_window_options", "mode") == "QUEUE_PACKETS" else 0)
    ]
    
    # Add flags if not already present
    for flag in time_window_flags:
        if flag not in build_flags:
            build_flags.append(flag)
    
    env.Replace(BUILD_FLAGS=build_flags)

# Register pre-build script
env.AddPreAction("buildprog", enable_time_window)