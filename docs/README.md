# Generic Phone Projection Middleware

A reusable, platform-agnostic middleware framework for phone projection systems (Android Auto, Apple CarPlay, proprietary IVI systems) written in modern C++20.

## Overview

This project demonstrates a clean architecture that separates platform-specific functionality from common middleware concerns, enabling easy adaptation to different automotive projection platforms.

### Key Features

- **Platform-Agnostic Core**: Session management, lifecycle, events, configuration, error handling
- **Adapter Pattern**: Clean interface boundary for platform-specific implementations
- **Android Auto Reference**: Complete simulated implementation for development/testing
- **Event-Driven Architecture**: Async pub/sub for all projection events
- **Configuration-Driven**: JSON-based vehicle profiles and platform configs
- **Modern C++20**: Uses standard library, no heavy dependencies
- **Comprehensive Tests**: Unit tests for core, platform, and adapter layers

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                         │
├─────────────────────────────────────────────────────────────────┤
│                    Middleware Core (Platform-Agnostic)          │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐             │
│  │ Session Mgr  │ │ Lifecycle Mgr│ │  Event Router│             │
│  └──────┬───────┘ └──────┬───────┘ └──────┬───────┘             │
│         │                │                │                      │
├─────────┼────────────────┼────────────────┼──────────────────────┤
│         ▼                ▼                ▼                      │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              IPlatformAdapter (Abstract Interface)        │   │
│  ├────────────┬────────────┬────────────┬──────────────────┤   │
│  │Connection  │  Resource  │   Input    │     UI Renderer   │   │
│  │  Manager   │  Manager   │  Handler   │    Capability     │   │
│  └────────────┴────────────┴────────────┴──────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
         │                │                │                │
         ▼                ▼                ▼                ▼
┌─────────────────────────────────────────────────────────────────┐
│                 Platform Adapter Implementations                 │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐             │
│  │ Android Auto │ │  CarPlay     │ │ Proprietary  │             │
│  │   Adapter    │ │   Adapter    │ │   Adapter    │             │
│  └──────────────┘ └──────────────┘ └──────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

## Project Structure

```
phone-projection-middleware/
├── CMakeLists.txt              # Root build configuration
├── cmake/                      # CMake modules
├── include/                    # Public headers
│   ├── core/                   # Core interfaces
│   │   └── interfaces.hpp      # All interface definitions
│   ├── platform/               # Platform abstraction
│   │   ├── platform_adapter.hpp
│   │   ├── connection_manager.hpp
│   │   ├── resource_manager.hpp
│   │   ├── input_handler.hpp
│   │   ├── ui_renderer.hpp
│   │   └── capability_provider.hpp
│   ├── android_auto/           # Android Auto adapter
│   │   └── android_auto_adapter.hpp
│   └── common/                 # Common utilities
│       └── common.hpp
├── src/                        # Implementation
│   ├── core/                   # Core middleware
│   ├── platform/               # Platform base classes
│   ├── android_auto/           # Android Auto implementation
│   └── common/                 # Common utilities
├── tests/                      # Unit tests
├── examples/                   # Demo applications
│   └── demo_app.cpp
└── docs/                       # Documentation
    ├── REQUIREMENTS.md
    ├── ADAPTATION_GUIDE.docs
    └── config_example.json
```

## Building

### Requirements

- CMake 3.20+
- C++20 compatible compiler (GCC 11+, Clang 13+, MSVC 19.30+)
- Dependencies (auto-fetched via CMake):
  - fmt (logging/formatting)
  - nlohmann/json (configuration)
  - GoogleTest (testing)

### Build Commands

```bash
# Configure
cmake -B build -S . -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON

# Build
cmake --build build --config Release

# Run tests
 ctest --test-dir build -C Release --output-on-failure


# Run demo
build\bin\Release\demo_app.exe
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | ON | Build shared libraries |
| `BUILD_TESTS` | ON | Build unit tests |
| `BUILD_EXAMPLES` | ON | Build example applications |
| `ENABLE_SANITIZERS` | OFF | Enable ASan/UBSan (Debug only) |
| `ENABLE_COVERAGE` | OFF | Enable code coverage |

## Usage

### Basic Example

```cpp
#include "core/interfaces.hpp"

using namespace phone_projection;
using namespace phone_projection::core;

int main() {
    // Create middleware
    auto middleware = create_projection_middleware();
    
    // Initialize
    middleware->lifecycle_manager()->initialize();
    
    // Load configuration
    middleware->config_manager()->load_config("config.json");
    
    // Create vehicle profile
    VehicleProfile profile;
    profile.vehicle_id = "my_vehicle";
    profile.make = "MyCar";
    profile.model = "2024";
    // ... set capabilities
    
    // Create and connect session
    auto session_result = middleware->create_and_connect_session(profile);
    if (session_result.is_ok()) {
        std::string session_id = session_result.value();
        // Session connected - handle events
    }
    
    // Subscribe to events
    middleware->event_router()->subscribe(EventType::TouchDown, 
        [](const Event& event) {
            // Handle touch input
        });
    
    // ... application logic
    
    // Cleanup
    middleware->lifecycle_manager()->shutdown();
    return 0;
}
```

### Event Handling

```cpp
auto& router = *middleware->event_router();

// Session events
router.subscribe(EventType::SessionConnected, [](const Event& e) {
    std::cout << "Session connected: " << e.session_id << std::endl;
});

// Input events
router.subscribe(EventType::TouchDown, [](const Event& e) {
    if (auto* te = dynamic_cast<const TouchEvent*>(&e)) {
        handle_touch(te->x, te->y);
    }
});

router.subscribe(EventType::RotaryClockwise, [](const Event& e) {
    if (auto* re = dynamic_cast<const RotaryEvent*>(&e)) {
        handle_rotary(re->steps);
    }
});

router.subscribe(EventType::VoiceCommand, [](const Event& e) {
    if (auto* ve = dynamic_cast<const VoiceEvent*>(&e)) {
        handle_voice(ve->transcript);
    }
});

// Resource events
router.subscribe(EventType::AudioFocusGained, [](const Event& e) {
    resume_audio();
});
```

### Resource Management

```cpp
auto* adapter = lifecycle->get_active_adapter_instance();
auto* resource_mgr = adapter->get_resource_manager();

// Request audio focus
auto audio = resource_mgr->request_resource(ResourceType::AudioFocus, session_id);
if (audio.is_ok()) {
    // Got audio focus, play media
    audio_token_ = audio.value().token;
}

// Request video surface
auto video = resource_mgr->request_resource(ResourceType::VideoSurface, session_id);
if (video.is_ok()) {
    // Got video surface, start rendering
}

// Release when done
resource_mgr->release_resource(audio_token_);
```

## Configuration

The middleware uses JSON configuration for vehicle profiles and platform settings:

```json
{
  "active_profile": "demo_vehicle_1",
  "vehicle_profiles": {
    "demo_vehicle_1": {
      "vehicle_id": "demo_vehicle_1",
      "make": "Generic",
      "model": "IVI System",
      "screen_width": 1280,
      "screen_height": 720,
      "capabilities": {
        "supports_touch": true,
        "supports_rotary": true,
        "supports_video": true,
        "max_video_width": 1920,
        "supported_codecs": ["H264", "VP8"]
      }
    }
  },
  "platform_configs": {
    "android_auto": {
      "enable_wireless": "false",
      "video_bitrate_kbps": "8000"
    }
  }
}
```

See `docs/config_example.json` for complete example.

## Adapting to New Platforms

See `docs/ADAPTATION_GUIDE.docx` for detailed instructions on creating new platform adapters.

### Quick Start

1. Create adapter class inheriting `PlatformAdapterBase`
2. Implement 5 component interfaces:
   - `IConnectionManager` - Transport & protocol
   - `IResourceManager` - Audio, video, input, nav focus
   - `IInputHandler` - Event translation
   - `IUiRenderer` - Template rendering
   - `ICapabilityProvider` - Capability declaration
3. Register with `REGISTER_ADAPTER(YourAdapter, your_platform)`
4. Add to CMake build

## Android Auto Adapter (Simulated)

The reference implementation includes a fully simulated Android Auto adapter:

- **Connection**: Simulated USB/Bluetooth handshake with state machine
- **Resources**: Audio focus, video surface, navigation, input focus
- **Input**: Touch, rotary, keyboard, voice simulation
- **UI**: Car App Library template rendering (Media, Navigation, List, etc.)
- **Capabilities**: Full Android Auto capability set

### Simulated Input (for testing)

```cpp
auto* adapter = lifecycle->get_active_adapter_instance();
auto* input = dynamic_cast<AndroidAutoInputHandler*>(adapter->get_input_handler());

// Simulate user interactions
input->simulate_touch(0.5f, 0.5f, TouchEvent::Action::Down, session_id);
input->simulate_rotary(RotaryEvent::Action::Clockwise, 3, session_id);
input->simulate_voice("Navigate home", 0.95f, true, session_id);
input->simulate_key(66, KeyEvent::Action::Press, session_id);  // Enter
```

## Testing

```bash
# Run all tests
 ctest --test-dir build -C Release --output-on-failure

# Run specific test
build/bin/tests/unit_tests --gtest_filter=SessionManagerTest.*

# With coverage
cmake -B build -S . -DENABLE_COVERAGE=ON
cmake --build build
ctest --test-dir build
# View coverage with lcov/genhtml
```
