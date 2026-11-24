# Generic Phone Projection Middleware - Requirements Document

## 1. Project Overview

### 1.1 Purpose
 POC to reusable, platform-agnostic middleware framework for phone projection systems (Android Auto, Apple CarPlay, etc.) that separates platform-specific functionality from common middleware concerns.

### 1.2 Scope
- **Selected Platform**: Android Auto (as primary reference implementation)
- **Target**: Proof-of-Concept demonstrating architecture viability
- **Future Extensibility**: Apple CarPlay, proprietary IVI systems, smart clusters

### 1.3 Key Principles
- **Separation of Concerns**: Platform adapters vs. core middleware
- **Interface-Driven Design**: Pure virtual interfaces for all platform interactions
- **Event-Driven Architecture**: Async message passing for projection events
- **Configuration-Driven**: Runtime configuration for different vehicle profiles

---

## 2. Android Auto Architecture Analysis

### 2.1 High-Level Architecture
```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Phone App     │────▶│  Projection     │────▶│   Vehicle IVI   │
│  (Android Auto) │     │   Protocol      │     │   (Head Unit)   │
└─────────────────┘     │  (USB/Bluetooth)│     └─────────────────┘
                        └─────────────────┘
```

### 2.2 Integration Constraints

| Constraint | Impact on Middleware Design |
|------------|----------------------------|
| **Proprietary Protocol** | Must abstract behind adapter interface; simulate for PoC |
| **USB/Bluetooth Transport** | Transport layer abstraction required |
| **Session Lifecycle** | Defined states: Disconnected → Connecting → Connected → Disconnecting |
| **Resource Management** | Audio focus, video surfaces, input focus |
| **Car App Library** | Template-based UI (not direct rendering) |
| **Safety Certification** | Driver distraction guidelines  |

### 2.3 Key Android Auto Concepts
- **CarService**: System service managing projection
- **CarAppService**: Entry point for projected apps
- **Session**: Represents active projection connection
- **Screen/Template**: UI rendering model
- **Input Events**: Rotary, touch, keyboard, voice

---

## 3. Common Middleware Responsibilities

### 3.1 Session Management
- Connection establishment/teardown
- Session state machine
- Capability negotiation
- Reconnection handling

### 3.2 Lifecycle Management
- Middleware initialization/shutdown
- Platform adapter loading/unloading
- Resource acquisition/release


### 3.3 Event Handling
- Input event routing (touch, rotary, keys, voice)
- System events (audio focus, video focus, navigation)
- Application lifecycle events
- Error/notification propagation

### 3.4 Configuration Management
- Vehicle profile configuration
- Platform-specific parameters
- Feature flags
- Runtime reconfiguration

### 3.5 Error Handling
- Structured error codes
- Recovery strategies
- Diagnostic logging

---

## 4. Platform-Independent Interfaces

### 4.1 Core Interface Hierarchy

```
IProjectionMiddleware (Facade)
├── ISessionManager
├── ILifecycleManager
├── IEventRouter
├── IConfigurationManager
└── IErrorHandler

IPlatformAdapter (Abstract Base)
├── IConnectionManager
├── IResourceManager
├── IInputHandler
├── IUiRenderer
└── ICapabilityProvider
```


## 5. Module Breakdown

| Module | Responsibility | Key Classes |
|--------|---------------|-------------|
| **core** | Middleware facade, session, lifecycle, events, config | `ProjectionMiddleware`, `SessionManager`, `LifecycleManager`, `EventRouter`, `ConfigManager` |
| **platform** | Abstract platform adapter interfaces | `IPlatformAdapter`, `IConnectionManager`, `IResourceManager`, `IInputHandler`, `IUiRenderer` |
| **android_auto** | Android Auto specific implementation | `AndroidAutoAdapter`, `AndroidAutoConnectionManager`, `AndroidAutoResourceManager`, `AndroidAutoInputHandler`, `AndroidAutoUiRenderer` |
| **common** | Shared utilities, types, logging, error codes | `Result`, `ErrorCode`, `Logger`, `Event`, `Config`, `Types` |

---

## 6. Development Plan

### Phase 1: Foundation (Week 1-2)
- [ ] Project structure and build system (CMake)
- [ ] Common types, error handling, logging
- [ ] Core interface definitions
- [ ] Basic event system

### Phase 2: Core Middleware (Week 2-3)
- [ ] SessionManager with state machine
- [ ] LifecycleManager with adapter loading
- [ ] EventRouter with pub/sub
- [ ] ConfigManager with vehicle profiles

### Phase 3: Platform Abstraction (Week 3-4)
- [ ] IPlatformAdapter and sub-interfaces
- [ ] Adapter factory/registry pattern
- [ ] Capability negotiation framework

### Phase 4: Android Auto Adapter & Integration & Demo (Week 4)
- [ ] Simulated connection manager (USB/Bluetooth)
- [ ] Resource manager (audio/video focus simulation)
- [ ] Input handler (rotary, touch, voice simulation)
- [ ] UI renderer (template simulation)
- [ ] End-to-end session simulation
- [ ] Demo application showing projection flow
- [ ] Configuration-driven vehicle profiles
- [ ] Documentation and adaptation guide



### 7 Project Structure
```
phone-projection-middleware/
├── CMakeLists.txt              # Root CMake
├── cmake/                      # CMake modules
├── include/                    # Public headers
│   ├── core/
│   ├── platform/
│   └── common/
├── src/
│   ├── core/                   # Middleware implementation
│   ├── platform/               # Platform abstraction
│   ├── android_auto/           # Android Auto adapter
│   └── common/                 # Shared utilities
├── tests/                      # Unit/integration tests
├── docs/                       # Documentation
└── examples/                   # Demo applications
```

---

## 8. Adaptation Strategy for Other Platforms

### 8.1 Apple CarPlay(Not in scope)
- Implement `CarPlayAdapter` inheriting `IPlatformAdapter`
- Map CarPlay session → `ISessionManager`
- Map CarPlay templates → `IUiRenderer`
- Handle CarPlay-specific: WiFi/BT transport, encryption

### 8.2 Proprietary IVI Systems
- Implement `ProprietaryAdapter`
- May need custom transport abstraction
- Vendor-specific resource management
- Custom UI rendering pipeline

### 8.3 Smart Cluster / Instrument Cluster
- Implement `ClusterAdapter`
- Simplified UI (tell-tales, gauges)
- High-priority safety-critical path
- Different input modalities

---

## 9. Success Criteria

1. **Compilation**: Clean build on Linux/Windows/macOS
2. **Architecture**: Clear separation between core and platform layers
3. **Simulation**: Demonstrates full projection session lifecycle
4. **Extensibility**: New adapter can be added without modifying core
5. **Documentation**: Complete API docs and porting guide
6. **Testing**: Unit tests for core, integration test for adapter