# 

# 

# 

# 

# 

# 

# 

# 

# PSAN4

# FYP Proposal

# Market Simulation for Algorithmic Trading

#  

# by

# BHANDARI Om Raj, CHOY Ping Hang Gordon, KWONG Wing Fung Josh

# PSAN4

#  

# Advised by

# Prof. Pedro SANDER 

#  

# Submitted in partial fulfillment of the requirements for COMP 4981

# in the

# Department of Computer Science

# The Hong Kong University of Science and Technology

# 2025-2026

# 

Table of Contents

[1\. Introduction	5](#1.-introduction)

[1.1 Overview	5](#1.1-overview)

[1.1.1 Introduction to Algorithmic Trading	5](#1.1.1-introduction-to-algorithmic-trading)

[1.1.2 Need for Trading Simulation	5](#1.1.2-need-for-trading-simulation)

[1.1.3 Project Contributions	5](#1.1.3-project-contributions)

[1.2 Objective	6](#1.2-objective)

[1.3 Literature Survey	8](#1.3-literature-survey)

[1.3.1 Limit order book architecture	8](#1.3.1-limit-order-book-architecture)

[1.3.2 System-level architecture	8](#1.3.2-system-level-architecture)

[1.3.3 Simulation methodologies	9](#1.3.3-simulation-methodologies)

[2\.  Methodology	10](#2.-methodology)

[2.1 Design	10](#2.1-design)

[2.1.1 User Access Layer: SDK & Graphical Interface (Objective 2 & 3\)	10](#2.1.1-user-access-layer:-sdk-&-graphical-interface-\(objective-2-&-3\))

[2.1.2 Network & Security Layer: FIX Gateways (Objective 4\)	11](#2.1.2-network-&-security-layer:-fix-gateways-\(objective-4\))

[2.1.3 Order Validation & Persistence Layer: Order Manager (Objective 4\)	12](#2.1.3-order-validation-&-persistence-layer:-order-manager-\(objective-4\))

[2.1.4 The Core: Matching Engine & LOB Data Structures (Objective 1\)	12](#2.1.4-the-core:-matching-engine-&-lob-data-structures-\(objective-1\))

[2.1.5 Distribution Layer: Market Data Processor (Objective 3\)	13](#2.1.5-distribution-layer:-market-data-processor-\(objective-3\))

[2.1.6 Simulation Layer: Agents & Market Regimes (Objective 5\)	14](#2.1.6-simulation-layer:-agents-&-market-regimes-\(objective-5\))

[2.2 Implementation	15](#2.2-implementation)

[2.2.1 Dual-Language SDK Development (C++ & Python)	15](#2.2.1-dual-language-sdk-development-\(c++-&-python\))

[2.2.2 FIX Gateway and Protocol Handling	15](#2.2.2-fix-gateway-and-protocol-handling)

[2.2.3 Order Manager and Persistence Layer	16](#2.2.3-order-manager-and-persistence-layer)

[2.2.4 Matching Engine and LOB Data Structures	16](#2.2.4-matching-engine-and-lob-data-structures)

[2.2.5 Market Data Processor	17](#2.2.5-market-data-processor)

[2.2.6 Agent Simulation Layer	17](#2.2.6-agent-simulation-layer)

[2.3 Testing	18](#2.3-testing)

[2.3.1 Test Plan and Methodology	18](#2.3.1-test-plan-and-methodology)

[2.3.2 Locating Logic Errors and Bugs	18](#2.3.2-locating-logic-errors-and-bugs)

[2.3.3 Simulation Realism	18](#2.3.3-simulation-realism)

[2.3.4 User Experience and Potential Failures	18](#2.3.4-user-experience-and-potential-failures)

[2.4 Evaluation	19](#2.4-evaluation)

[2.4.1 Limit Order Book Efficiency (Objective 1\)	19](#2.4.1-limit-order-book-efficiency-\(objective-1\))

[2.4.2 User Accessibility & Visual Analytics (Objective 2 & 3\)	19](#2.4.2-user-accessibility-&-visual-analytics-\(objective-2-&-3\))

[2.4.3 Distributed System Architecture (Objective 4\)	19](#2.4.3-distributed-system-architecture-\(objective-4\))

[2.4.5 Simulation Effectiveness (Objective 5\)	19](#2.4.5-simulation-effectiveness-\(objective-5\))

[3 Project Planning	20](#3-project-planning)

[3.1 Distribution of Work & Gantt Chart	20](#3.1-distribution-of-work-&-gantt-chart)

[4 Required hardware & software	21](#4-required-hardware-&-software)

[4.1 Hardware	21](#4.1-hardware)

[4.2 Software	21](#4.2-software)

[5 References	22](#5-references)

[6 Appendix	23](#6-appendix)

[6.1 Minutes of Project Meeting	23](#6.1-minutes-of-project-meeting)

[6.1.1 Minutes of the 1st Project Meeting	23](#6.1.1-minutes-of-the-1st-project-meeting)

[6.1.2 Minutes of the 2nd Project Meeting	24](#6.1.2-minutes-of-the-2nd-project-meeting)

[6.1.3 Minutes of the 3rd Project Meeting	25](#6.1.3-minutes-of-the-3rd-project-meeting)

[6.1.4 Minutes of the 4th Project Meeting	26](#6.1.4-minutes-of-the-4th-project-meeting)

[6.1.5 Minutes of the 5th Project Meeting	27](#6.1.5-minutes-of-the-5th-project-meeting)

[6.1.6 Minutes of the 6th Project Meeting	29](#6.1.6-minutes-of-the-6th-project-meeting)

[6.1.7 Minutes of the 7th Project Meeting	30](#6.1.7-minutes-of-the-7th-project-meeting)

[6.1.8 Minutes of the 8th Project Meeting	31](#6.1.8-minutes-of-the-8th-project-meeting)

[6.1.9 Minutes of the 9th Project Meeting	32](#6.1.9-minutes-of-the-9th-project-meeting)

[6.1.10 Minutes of the 10th Project Meeting	33](#6.1.10-minutes-of-the-10th-project-meeting)

[6.1.11 Minutes of the 11th Project Meeting	34](#6.1.11-minutes-of-the-11th-project-meeting)

[6.2 Explanation on Limit Order Books	36](#6.2-explanation-on-limit-order-books)

# 

# 1\. Introduction	 {#1.-introduction}

## 1.1 Overview {#1.1-overview}

### 1.1.1 Introduction to Algorithmic Trading {#1.1.1-introduction-to-algorithmic-trading}

Financial markets are complex, dynamic systems where algorithmic trading plays a crucial role in price discovery, liquidity provision, and market efficiency. Hong Kong, as one of the world’s leading financial hubs, attracts top institutions and professionals who leverage advanced trading algorithms to remain competitive, including investment banks, hedge funds, and market makers. However, there is minimal access to high-quality, open-source algorithmic trading platforms that allow newcomers, such as university students, to interact with limit order books. 

### 1.1.2 Need for Trading Simulation {#1.1.2-need-for-trading-simulation}

We were inspired by algorithmic trading competitions such as IMC Prosperity that demonstrate the educational value of simulated markets. Participants design systematic strategies and deploy them against unseen market data containing IMC-proprietary bots that are configured with diverse behaviours, such as market making. Unfortunately, the code is closed-source and inaccessible for independent learning. This project seeks to fill the gap by creating an educational trading simulator. 

### 1.1.3 Project Contributions {#1.1.3-project-contributions}

This project contributes an extensible algorithmic trading simulator together with a systematic empirical study of limit order book implementations, with particular emphasis on data structure design and low-latency performance characteristics in high-volume settings. The simulator simulates the environment of real-world trading by supporting multiple assets, heterogeneous agent populations, and configurable market regimes, thereby providing a controlled testbed for both strategy evaluation and microstructure experimentation.

The primary technical contribution is the design, implementation, and comparative evaluation of several financial exchange backends that realize the same matching logic over different underlying data structures, such as node-based containers (e.g. balanced binary search tree–style price maps with per-level linked queues) versus contiguously stored containers (e.g. vector‑like arrays of price levels with in-place order queues). This enables a direct comparison of how memory layout, pointer indirection, CPU cache locality affect core operations such as insertion, cancellation, best‑quote retrieval, and full-book traversal. We also experiment and implement modular architectures which utilize concurrency techniques such as lock-free ring buffers.

The secondary educational contribution is the parameterizable multi-scenario configurations that expose microstructural variables to the user, such as agent order-submission intensities, volatility and regime dynamics, tick size, and artificial latency. These scenarios support different classes of agents (e.g. market makers, noise traders, momentum and value strategies) and can be instantiated per symbol and per “trading day”. Alongside the simulations, the project contributes a visual analytics layer that presents a time series of prices, spreads, depths, and selected microstructural indicators derived from the simulated LOB state. The simulation and visualizations make the platform suitable for instructional use in courses on market microstructure and algorithmic trading.

## 1.2 Objective {#1.2-objective}

The goal of the project is to provide a safe and realistic venue for beginners in algorithmic trading to learn and test their skills. To facilitate this goal, it is important to design and implement a robust simulated market platform for real time trading. Such a simulated market platform will then allow users to test their trading strategies in realistic market scenarios and refine their skills through trial and error. Therefore, our objectives will revolve around realistic market simulation and the ability to handle large volumes of trades as a real life stock exchange should be.

The main objectives that the project would focus on are as follows:

1. To build an efficient limit order book that could handle incoming orders at lowest latency possible.  
2. To provide an intuitive method for users to submit orders.  
3. To provide a clear and clean interface for visualising market information.  
4. To design an efficient and robust system architecture that can handle high user traffic.  
5. To implement realistic and configurable market simulation mechanisms.

For objective 1 and 4, we conducted research on existing limit order book designs, stock exchange architecture diagrams and C++ conferences (cppcon). The researched results are now being benchmarked to arrive at the most efficient solutions. To achieve objective 2, we designed and implemented a Software Development Kit (SDK) in C++, and will implement Python bindings as it is one of the most beginner friendly programming languages. We studied interfaces of commonly used brokerage applications, including IBKR and Webull, to satisfy objective 3\. Our study allowed us to pinpoint necessary information to display and understand how data should be presented in a clear manner. For objective 4, we are in progress of building a distributed architecture that can support a high volume of user submitted orders. Finally for objective 5, we will read research papers that proposed theoretically plausible methods of market simulation. We have funneled down to several methods that we will closely test and monitor.

## 1.3 Literature Survey {#1.3-literature-survey}

This literature survey examines existing work on the three major components of a financial exchange: the limit order book, system-level architecture, and simulation methodologies.

### 1.3.1 Limit order book architecture  {#1.3.1-limit-order-book-architecture}

The limit order book sits at the core of an exchange. In real exchanges, limit order books handle message rates up to 500,000 per second with microsecond latencies \[3\]. Correct and efficient implementation is crucial. Essentially, it must implement add, cancel and execute operations for orders, with efficient queries such as getting the best bid price and ask price, current volume information and the status of the order submitted by a specific user. For a detailed explanation, refer to [Appendix 6.2](#6.2-explanation-on-limit-order-books). Current designs utilise binary trees as the primary data structure to store orders, with each node referring to limit price. Each node then contains a doubly linked list of order nodes corresponding to those that users submit. To solve the issue of efficient queries, two hashmaps store references to limit price nodes of the tree and order nodes in the linked list respectively \[5\]. However, current designs do not explicitly define which type of binary tree to use for keeping the trees balanced, as the average time complexity may grow if left unmaintained. However, the literature only considers theoretical time complexity, does not consider empirical performance, such as benchmarked latency and its distributions. It also does not consider issues of computer architectures, such as cache locality and cache line optimization.

### 1.3.2 System-level architecture {#1.3.2-system-level-architecture}

Modern exchanges handle thousands of assets, each with its own limit order book, and multiple concurrent users submitting orders. An exchange faces upwards of 3,000,000 messages per second at peak levels \[3\]. Most exchanges do not reveal their source code due to competitive advantages. Hence, existing literature only describes the architecture of exchanges at a high level \[1\]. Users first send messages regarding their order intentions to an exchange gateway layer which performs validation on message integrity and is run on a separate computer from the core limit order books.  This layer contains multiple parallel gateways for users to choose and send messages into. To handle simultaneous messages and introduce a level of randomness to message handling, a sequencer first materialises the incoming messages into a single stream of ordered messages. This stream is then passed on to the limit order books for order matching, addition or cancellation. However, current literature does not concretely define the implementation details of this architecture, such as the message and network connection protocols, load-balancing mechanisms and inter-service communication.

### 1.3.3 Simulation methodologies {#1.3.3-simulation-methodologies}

It is often useful to include order simulation to model behaviours of market participants. Existing exchange simulation tools provide historical market data replay mechanisms and simple random order generation to model noise, but do not provide support for advanced mathematical and AI-based models \[4\]. By contrast, quantitative finance literature provides limit order book simulation frameworks for various categories, such as point process models using the Poisson process and Hawkes process, but does not provide source code to implement such processes in a stock exchange program \[2\].

The surveyed literature demonstrates useful designs for the core components for our exchange project. However, most works remain high-level in terms of implementation. Our project aims to integrate all three of these component designs to implement an efficient and scalable platform that is educational with support for simulation methods.

# 

# 2\.  Methodology {#2.-methodology}

## 2.1 Design {#2.1-design}

![][image1]

### 2.1.1 User Access Layer: SDK & Graphical Interface (Objective 2 & 3\) {#2.1.1-user-access-layer:-sdk-&-graphical-interface-(objective-2-&-3)}

* Trading SDK: We have provided a programmatic interface for developers. It abstracts complex networking logic into simple function calls, such as “submit limit order”, “submit market order” and “cancel order given order ID”. To satisfy the goal of being beginner-friendly, we will implement a wrapper layer using beginner-friendly languages whilst maintaining high performance.  
* Graphical Trading Interface: We designed a web-based dashboard inspired by professional brokerages (e.g., IBKR). It provides the "visual analytics layer" mentioned in Objective 3, rendering real-time market depth and price action.

![][image2]

* Data Input: Users are now able to submit order requests by specifying fields like Symbol, Side (Bid/Ask), OrderType (Limit/Market), Price, and Quantity.

### 2.1.2 Network & Security Layer: FIX Gateways (Objective 4\) {#2.1.2-network-&-security-layer:-fix-gateways-(objective-4)}

Mirroring real-world exchange architecture, all incoming user requests first hit the Gateways to perform simple message validation, before requests are parsed into internal representations.

* Functionality: This acts as a protocol translator. It handles session management (logins/logouts) and ensures that incoming requests are syntactically correct before they enter the internal network.  
* Performance: By decoupling the connection handling from the core logic, the system can scale horizontally to handle high user traffic (Objective 4\) without slowing down the matching process. For example, additional gateways can be instantiated in new machines when demand is high.

### 

### 2.1.3 Order Validation & Persistence Layer: Order Manager (Objective 4\) {#2.1.3-order-validation-&-persistence-layer:-order-manager-(objective-4)}

Once a request passes the gateway, it moves to the Order Manager, which serves as the "central hub" of the backend. 

* Semantic Validation: The manager performs critical checks that the gateway cannot, such as Balance Checking (does the user have enough balance to place this buy order?) and Position Tracking (does this user have enough inventory to sell?)  
* Database Integration: It interfaces with two distinct databases:  
  * Relational database: Used for data like user profiles, credentials, and account balances.  
  * Time-Series: Used for high frequency order data. We persist every order and trade event here to ensure a robust audit trail and to allow for historical simulation replays.

After performing the above tasks, potentially asynchronously, it sends the validated order requests to the matching engine. It passes on trade confirmations back to the gateway, notifying the user that sent it.

### 2.1.4 The Core: Matching Engine & LOB Data Structures (Objective 1\) {#2.1.4-the-core:-matching-engine-&-lob-data-structures-(objective-1)}

This is the performance-critical heart of the exchange where Objective 1 is realized.

* Processing Flow: The engine receives a validated request, attempts to match it against the opposite side of the book, generates Trade events if a match occurs and sends them to the Market Data Processor, and updates the state of the Limit Order Book.  
* Matching Logic: We used the Price-Time Priority algorithm, meaning orders with the best price are matched first, whereas orders with the same price are executed on a first-come-first-served basis.  
* Data Structure Design: Based on our research, we initially attempted a theoretically optimal LOB implementation based on time complexity. The Bid and Ask sides are each a balanced Binary Search Tree (BST), allowing O(log N) access to each Price Level. Each Price Level is a tree node containing a Doubly-Linked List holding individual Orders, complemented by a Hashmap mapping Order ID to actual Order objects for ease of O(1) order cancellation. However, we realized this approach does not consider hardware and CPU architecture, where cache optimization affects throughput and latency heavily in terms of distribution. We will compare and benchmark a new design: using dynamic arrays alongside linear search and binary search algorithms to measure latency against empirical data. 

### 2.1.5 Distribution Layer: Market Data Processor (Objective 3\) {#2.1.5-distribution-layer:-market-data-processor-(objective-3)}

The output from the Matching Engine is raw and high-velocity. The Market Data Processor transforms this into human-readable information.

* Inter-Process Communication (IPC): To maintain the performance gains achieved by the matching engine, the communication channel between the engine and the processor is designed using a Lock-Free Ring Buffer. This architecture is chosen to eliminate the overhead of traditional synchronization primitives:  
  * Non-Blocking Synchronization: We utilised a busy-wait (polling) mechanism for consumers to ensure immediate processing of new events. This design anticipates future thread-to-CPU pinning (CPU Affinity) to prevent context-switching latencies.  
  * Bit Manipulation Optimization: The buffer capacity is constrained to a power of two. This allows the system to replace expensive modulo operations with a bitwise AND operation for index wrapping, significantly reducing clock cycles per message.  
  * Hardware-Aware Cache Optimization: To prevent false sharing, the design incorporates memory alignment and padding (64 bytes for x86 architectures). This ensures that the producer and consumer indices reside on separate cache lines.  
* Aggregation: It converts individual trade events into orderbook deltas, which are (Price, Quantity) pairs for each Price Level, computing the difference in orderbook states aggregated over a fixed time interval.  
* Broadcasting: It publishes structured LOB data back to the client host, where the clients can visualize via the Graphical Interface∫and process using the SDK, completing the feedback loop for the user.

### 2.1.6 Simulation Layer: Agents & Market Regimes (Objective 5\) {#2.1.6-simulation-layer:-agents-&-market-regimes-(objective-5)}

To provide a "realistic venue," and educational trading partners, the system includes an Agent layer that interacts with the Gateways just like human clients. The models are categorised into two types:

Point process models

We plan to utilise existing models available in literature, including:

* Poisson Process, a basic arrival model with constant rate (lambda term) as a baseline  
* Hawkes Process, a self-exciting process (meaning an event’s occurrence increases the probability of future events) that captures order clustering and feedback effects

Agent-based models

The aim here is to simulate heterogenous market players with commonly known but distinct behaviours, including:

* Market makers who provide liquidity and consider inventory management  
* Informed traders who have access to private information regarding the fair price of an asset  
* Noise traders who trade randomly to represent market players without access to private information

## 2.2 Implementation {#2.2-implementation}

The following figure details how the data is transformed autonomously from end-to-end:  
![][image3]

### 2.2.1 Dual-Language SDK Development (C++ & Python) {#2.2.1-dual-language-sdk-development-(c++-&-python)}

To balance performance with accessibility, the Software Development Kit (SDK) will be implemented using a multi-language approach:

* Core C++ SDK: This version has been built for high-performance execution, providing the foundational logic for order construction and network communication.  
* Python Wrapper: Utilizing libraries like pybind11, we will wrap the C++ SDK to provide a native Python interface. This will satisfy our educational requirement, allowing beginners to deploy strategies without managing low-level C++ complexities.

### 2.2.2 FIX Gateway and Protocol Handling {#2.2.2-fix-gateway-and-protocol-handling}

* QuickFIX Integration: We utilized the open-source QuickFIX library to handle the Financial Information eXchange (FIX) protocol. It manages session persistence, message sequence numbers, and basic syntax validation.  
* FIX Message Parsing: Incoming messages follow the tag-value format. For example, a "New Order Single" might appear as: 8=FIX.4.2|35=D|54=1|38=100|44=150.50|... where 35=D identifies the message type.  
* Serialization: Requests are serialized using Google Protocol Buffers (protobuf) for compact binary transmission via WebSockets to the Order Manager.  

### 2.2.3 Order Manager and Persistence Layer {#2.2.3-order-manager-and-persistence-layer}

The Order Manager acts as the orchestrator for semantic validation and data persistence.

* Balance Validation & Position Tracking: We will implement SQL queries against a PostgreSQL relational database to verify user account balances and inventory before an order is marked as "valid". The database has yet to be constructed.  
* Persistence Strategy: All requests will be asynchronously written to QuestDB, a high-performance time-series database. This will ensure every order and trade is timestamped and persisted for auditability and simulation replay. The database has yet to be constructed.  
* Serialization: Similar to the gateway, validated orders are serialized via protobuf and dispatched to the Matching Engine over WebSockets.

### 2.2.4 Matching Engine and LOB Data Structures {#2.2.4-matching-engine-and-lob-data-structures}

This core component was implemented with a focus on algorithmic efficiency and O(1) or O(log N) operations.

* Limit Order Book (LOB) Design: We utilized std::map, which is a Red-Black Tree under the hood, to represent the bid and ask sides of the book, where each key is a price level. Each price level points to an std::list of Order objects to maintain time priority, and can be accessed in O(log N) time complexity.  
* Efficient Cancellations: To achieve fast order removal, we utilized an std::unordered\_map, which is a hash map implementation, that maps order\_id to the specific iterator in the std::list, which is a doubly linked list under the hood. This allows for O(1) cancellation by bypassing a full book search.  
* Inter-Process Communication (IPC): Upon a successful match, trade events are loaded into a Shared Memory file to minimize the overhead of copying data between processes.

### 2.2.5 Market Data Processor {#2.2.5-market-data-processor}

* Atomic Shared Memory Access: We used the \<sys/mman.h\> system call for memory mapping. To ensure thread safety and high throughput without heavy locks, we utilized the \<atomic\> library with explicit memory ordering. We specifically applied std::memory\_order\_acquire for loads and std::memory\_order\_release for stores to synchronize data across the memory map, rather than enforcing sequential consistency, which incurs overhead.  
* JSON Broadcasting: We integrated the nlohmann/json library to serialize order book deltas and trade events into JSON format. The Processor maintains persistent WebSocket connections to push these updates back to the Clients and Agents in real-time.

### 2.2.6 Agent Simulation Layer {#2.2.6-agent-simulation-layer}

The simulation environment is populated by automated agents designed via statistical modeling.

* Realistic Behavior: Agents will be programmed to mimic diverse market participants (e.g., market makers or noise traders) by submitting FIX messages that follow specific probability distributions for price and volume. We have yet to begin implementation.

## 2.3 Testing {#2.3-testing}

### 2.3.1 Test Plan and Methodology {#2.3.1-test-plan-and-methodology}

We utilize the Catch2 library for our unit testing framework due to its lightweight and expressive nature.

* Unit Testing: We verify the core components, including Client SDK, Gateway, Order Manager, Matching Engine and Market Data Processor, by simulating diverse order scenarios. This includes testing the Price-Time Priority algorithm to ensure orders are filled in the correct sequence.  
* Integration Testing: We test the data flow from the Client SDK through the FIX Gateway and Order Manager to ensure that Protobuf serialization and WebSocket communication maintain data integrity. Trade events emitted by the Matching Engine are also tested to ensure the end-to-end flow is correct. The frontend UI is manually checked for any visual discrepancies.  
* Performance Benchmarking: To determine if the system is efficient, we will measure "tick-to-trade" latency, which is the time between an order arriving at the Gateway and the corresponding trade event emitted by the Matching Engine.

### 2.3.2 Locating Logic Errors and Bugs {#2.3.2-locating-logic-errors-and-bugs}

Testing helped us identify critical flaws that were not initially apparent. One such flaw was the LOB invariant violation. A significant logic error was discovered where the Matching Engine attempted to match against the *worst* ask price rather than the *best*. This occurred because the engine incorrectly accessed the end of the ascending price list instead of the beginning. While simple unit tests missed this, a random order submission simulation revealed that the "Ask" prices were lower than "Bids," a clear violation of market invariants. This encouraged us to write more detailed test cases, as well as adopt a “Design by Contract” principle to catch programming bugs. This includes preconditions, postconditions and invariant assertions.

### 2.3.3 Simulation Realism {#2.3.3-simulation-realism}

To maximize educational quality from the agent simulations, we will run the various agents for prolonged periods of time (1 hour and 4 hours respectively) to measure short-term and long-term order-submission behavior respectively. We plan to plot the price graphs and order flows in Matplotlib for further analysis.

### 2.3.4 User Experience and Potential Failures {#2.3.4-user-experience-and-potential-failures}

From a user’s perspective, several issues could impact the effectiveness of the platform:

* Stale Market Data: If the Market Data Processor fails to aggregate LOB deltas correctly, users might see "ghost" liquidity on the Graphical Interface that does not actually exist in the Matching Engine.   
* SDK Connectivity: Beginners using the Client SDK might face unhandled exceptions if the underlying WebSocket connection to the Gateway is interrupted. Hence, connection drop rates will be measured after deploying the server on an EC2, and submitting trades from client hosts across the network. We will measure connection drop rates and speed against an increasing number of client connections.

## 2.4 Evaluation {#2.4-evaluation}

### 2.4.1 Limit Order Book Efficiency (Objective 1\) {#2.4.1-limit-order-book-efficiency-(objective-1)}

To evaluate the efficiency of our Limit Order Book (LOB), we are currently comparing our current node-based container implementation against real-life exchanges, using openly obtainable exchange data, and general metrics such as operations per second per assert.

### 2.4.2 User Accessibility & Visual Analytics (Objective 2 & 3\) {#2.4.2-user-accessibility-&-visual-analytics-(objective-2-&-3)}

We are assessing the usability of our platform by comparing it to similar projects like IMC Prosperity. This includes comparison of the UI/UX-friendliness, responsiveness and portability across multiple computers.

### 2.4.3 Distributed System Architecture (Objective 4\) {#2.4.3-distributed-system-architecture-(objective-4)}

We will evaluate the robustness of our backend by comparing it against the average number of concurrently supported users in real-life exchanges. 

### 2.4.5 Simulation Effectiveness (Objective 5\) {#2.4.5-simulation-effectiveness-(objective-5)}

We will gather user feedback to evaluate the realism of our agent simulations once implemented. This includes a questionnaire to Computer Science students and Finance students as the target users.

# 3 Project Planning {#3-project-planning}

## 3.1 Distribution of Work & Gantt Chart	 {#3.1-distribution-of-work-&-gantt-chart}

*G \- Gordon    J \- Josh    O \- Om*

| Task | PIC | Aug | Sep | Oct | Nov | Dec | Jan | Feb | Mar | Apr |
| :---- | :---: | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |
| Perform Literature Survey | O |  |  |  |  |  |  |  |  |  |
| Write Proposal Report | G |  |  |  |  |  |  |  |  |  |
| Design & Implement Limit Order Book | J |  |  |  |  |  |  |  |  |  |
| Design & Implement Gateway | O |  |  |  |  |  |  |  |  |  |
| Design & Implement Order Manager | G |  |  |  |  |  |  |  |  |  |
| Design & Implement Market Data Processor | J |  |  |  |  |  |  |  |  |  |
| Design & Implement Trade Interface and SDK | J |  |  |  |  |  |  |  |  |  |
| Test Limit Order Book | G |  |  |  |  |  |  |  |  |  |
| Test Gateway | O |  |  |  |  |  |  |  |  |  |
| Test Order Manager | G |  |  |  |  |  |  |  |  |  |
| Test Market Data Processor | J |  |  |  |  |  |  |  |  |  |
| Test Trade Interface and SDK | J |  |  |  |  |  |  |  |  |  |
| Design & Implement System Architecture & Infrastructure | G |  |  |  |  |  |  |  |  |  |
| Design & Implement Traffic & Connection Management | O |  |  |  |  |  |  |  |  |  |
| Test System Architecture & Infrastructure | G |  |  |  |  |  |  |  |  |  |
| Test Traffic & Connection Management | O |  |  |  |  |  |  |  |  |  |
| Design, Implement & Test Market Simulation Models | O |  |  |  |  |  |  |  |  |  |
| Write Progress Report | J |  |  |  |  |  |  |  |  |  |
| Write Final Report | G |  |  |  |  |  |  |  |  |  |
| Record Presentation Video | J |  |  |  |  |  |  |  |  |  |
| Prepare Presentation Deck | O |  |  |  |  |  |  |  |  |  |

# 

# 4 Required hardware & software {#4-required-hardware-&-software}

## 4.1 Hardware {#4.1-hardware}

* 4 AWS EC2 T3 instances with   
  * at least 2 gigabytes of RAM each.  
  * X86-based CPU architecture.

## 4.2 Software {#4.2-software}

* Figma for UI design  
* React and TypeScript for frontend development  
* C++23 as core programming language  
* Amazon RDS with PostgreSQL  
* QuestDB


# 5 References {#5-references}

\[1\] M. Aquilina, E. Budish, and P. O’Neill, “Quantifying the High-Frequency Trading ‘Arms Race’: A New Methodology and Estimates,” Financial Conduct Authority, Occasional Paper no. 50, 2020\. \[Online\]. Available: [https://www.fca.org.uk/publication/occasional-papers/occasional-paper-50.pdf](https://www.fca.org.uk/publication/occasional-papers/occasional-paper-50.pdf). \[Accessed: Oct. 1, 2025\].

\[2\] K. Jain, N. Firoozye, J. Kochems, and P. Treleaven, "Limit Order Book Simulations: A Review," *arXiv preprint arXiv:2402.17359*, Feb. 2024\.

\[3\] B. Nigito, "How to Build an Exchange," Jane Street Tech Talks, Jane Street Group, LLC, 2025\. \[Online\]. Available: [https://www.janestreet.com/tech-talks/building-an-exchange/](https://www.janestreet.com/tech-talks/building-an-exchange/). \[Accessed: Oct. 1, 2025\].

\[4\] Quod Financial, "Open Source Multi-Asset Market Simulator: Test Trading Strategies \- Quant Replay," QuantReplay, 2025\. \[Online\]. Available: [https://quantreplay.com/](https://quantreplay.com/). \[Accessed: Oct. 1, 2025\].

\[5\] wkselph, "How to Build a Fast Limit Order Book," *WK's High Frequency Trading Blog*, Feb. 15, 2011\. \[Online\]. Available: [https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/). \[Accessed: Oct. 1, 2025\].

# 6 Appendix {#6-appendix}

## 6.1 Minutes of Project Meeting {#6.1-minutes-of-project-meeting}

### 6.1.1 Minutes of the 1st Project Meeting {#6.1.1-minutes-of-the-1st-project-meeting}

Date:        May 5, 2025  
Time:       2:00 pm  
Place:       Room 3504  
Present:    Gordon, Josh, Om, Prof. Sander  
Absent:    None  
Recorder: Josh

1. Approval of minutes  
   There were no minutes to approve since this was the first formal group meeting.  
2. Report on progress  
   2.1  All members have read and understood the Final Year Project instructions.  
   2.2  All members have researched several potential FinTech-related topics.  
3. Discussion items  
   3.1  Om introduced three topics that we were interested in working on, including market simulation for educational algorithmic trading, design and implementation of an exchange architecture, and simulation of a limit order book for high-frequency trading. Om asked whether Prof. Sander was willing to supervise our Final Year Project.  
   3.2  Gordon pointed out that the first and the third topics were similar, and we might consider merging them into a new topic, for example, the implementation of an algorithmic trading platform that provided a simulated market environment with a highly efficient limit order book design.   
   3.3  Prof. Sander showed interest in the topics mentioned. He pointed out that the focus of these topics was diverse, from benchmarking the efficiency of different data structures for the limit order book to system design. He expressed his willingness to supervise our project and suggested that we write a draft proposal with details to provide more context before narrowing down the direction in which we wanted to move forward.  
4. Goals for the coming month

All members will do further research on the three topics closely  
A draft proposal will be sent to Prof. Sander by May 11, 2025\.

5. Meeting adjournment and next meeting  
   The meeting was adjourned at 2:45 pm.  
   The next meeting will be at 2:00 pm on May 12th at Room 3504\.

### 6.1.2 Minutes of the 2nd Project Meeting {#6.1.2-minutes-of-the-2nd-project-meeting}

Date:        May 12, 2025  
Time:       2:00 pm  
Place:       Room 3504  
Present:    Josh, Om, Prof. Sander  
Absent:    Gordon  
Recorder: Josh

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   2.1  All members have done further analysis on the proposed topics during the last meeting.  
   2.2  Om drafted the proposal explaining each topic in detail, which was sent to Prof. Sander.  
3. Discussion items  
   3.1  Prof. Sander said that he read the proposal and found it interesting. He suggested that we spend the coming summer on research because there was time remaining before we submitted the final proposal. He also advised us to set up a Zoom channel for easier communication.  
   3.2  Om agreed with Prof. Sander’s advice.  
4. Goals for the coming three months

All members will do a deeper analysis and write up a proposal with the finalised topic.

5. Meeting adjournment  
   The meeting was adjourned at 2:30 pm.

### 6.1.3 Minutes of the 3rd Project Meeting {#6.1.3-minutes-of-the-3rd-project-meeting}

Date:        Aug 21, 2025  
Time:       8:30 am  
Place:       Economy Class of Flight UO612, from Hong Kong to Fukuoka, Japan  
Present:    Gordon, Om  
Absent:    Josh  
Recorder: Gordon

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   2.1  All members narrowed down the final topic to market simulation for algorithmic trading, with high efficiency implementation of some core components of an exchange architecture.  
   2.2  All members have started working on the final proposal.  
3. Discussion items  
   3.1  Gordon pointed out that the coverage of our finalised topic was a bit large, so it made more sense to divide the development plan into different phases.  
   3.2  Om suggested three phases. At phase 1, the core market mechanism is implemented, including the limit order book, trade interface, and historical data handler, enabling users to host locally and backtest their own strategy with static historical market data. At phase 2, the system evolves into a centralized server, and the users submit orders by communicating with the server via HTTP or WebSocket requests. At phase 3, we implement a model that can simulate behaviours of different parties in the market, including market makers, institutions, individual investors, etc, to make the market environment more realistic.  
   3.3 Gordon estimated the man-hours required for each task.  
4. Goals for the coming week

All members will do competitor analysis, as well as research on existing exchange architectures.

5. Meeting adjournment  
   The meeting was adjourned at 9:30 am.

### 6.1.4 Minutes of the 4th Project Meeting {#6.1.4-minutes-of-the-4th-project-meeting}

Date:        Aug 23, 2025  
Time:       8:00 pm  
Place:       Royal Park Canvas, Fukuoka, Japan  
Present:    Josh, Gordon, Om  
Absent:    None  
Recorder: Josh

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   2.1  Gordon has done a competitor analysis.  
   2.2  Josh and Om have researched existing exchange architectures.  
3. Discussion items  
   3.1  Gordon shared his findings. He mentioned that many crypto exchanges, like Binance and OKX, provide a set of trading API for simulation only. Apart from that, IMC holds an annual trading competition named IMC Prosperity. The participants are given a Python SDK to trade with a set of pre-defined market bots and try to earn as much as possible.  
   3.2  Josh said that with their market data and exchange infrastructure, these exchanges can provide very similar services and questioned how we can have an edge against them.  
   3.3  Om shared the research results of some current exchange architectures. He pointed out that the limit order book is one of the most important core components, which we should put much effort into developing for the best performance. Furthermore, a typical exchange architecture also includes other components, including the exchange gateway, sequencers, market data processor, and so on. He said it was not feasible to implement all components within the scope of this project, and we might need to choose between them based on our use cases.  
4. Goals for the coming week  
   4.1  All members will continue working on the proposal report.  
   4.2  All members will study closely to narrow down the required components for the project.  
5. Meeting adjournment  
   The meeting was adjourned at 9:30 pm.

### 6.1.5 Minutes of the 5th Project Meeting {#6.1.5-minutes-of-the-5th-project-meeting}

Date:        Sep 19, 2025  
Time:       3:00 pm  
Place:       Room 4368  
Present:    Josh, Gordon  
Absent:    Om  
Recorder: Josh

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   2.1  All members have worked together on finishing the proposal report.  
   2.2  All members have started learning advanced C++ techniques.  
3. Discussion items  
   3.1  Gordon suggested we start designing the architecture of the local backtester for phase 1 of the project. He proposed a client-server architecture where the users interact with the matching engine via API because that is what we can reuse for phase 2\.  
   3.2  Josh said that with the client-server design the user is not guaranteed to get the same backtest result every time because of asynchronization, which was counter intuitive as a backtester.  
   3.3  Gordon agreed with Josh’s point, and suggested a mechanism to synchronize the client with the server which only applies to backtester only.  
   3.4  Josh expressed his concern about the overhead by doing so. But he agreed that it is important to produce reusable components and interfaces for later phases. He then refined the architecture diagram.  
4. Goals for the coming week  
   4.1  All members will continue learning C++.  
   4.2  All members will start designing the components they are responsible for developing.  
5. Meeting adjournment  
   The meeting was adjourned at 5:00 pm. 

### 6.1.6 Minutes of the 6th Project Meeting {#6.1.6-minutes-of-the-6th-project-meeting}

Date:        Oct 1, 2025  
Time:       10:00 am  
Place:       Room 4368  
Present:    Josh, Gordon, Om  
Absent:    None  
Recorder: Om

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   2.1  All members have designed their components.  
   2.2  All members have started learning advanced C++ techniques.  
3. Discussion items  
   3.1  Gordon shared his design of the matching engine. He suggested each matching engine should hold multiple LOBs, each of which is a node-based data structure for O(log n) time complexity for all major operations like search, insertion and deletion.  
   3.2  Om referred to a research paper and argued that low time complexity does not imply high performance in reality. Using std::vector is more cache-friendly and performance boosting.  
   3.3  Josh shared his design of the SDK for clients to interact with our system. Om shared his design of the gateway.  
   3.4  Josh and Om discussed the protocol and parameters used for the communication between the client SDK and the gateway. FIX4.2 was agreed to be used.  
4. Goals for the coming week  
   4.1  All members will start developing the components.  
   4.2  Gordon will benchmark the performance difference between a node-based data structure and a linear std::vector for the matching engine.  
5. Meeting adjournment  
   The meeting was adjourned at 1:00 pm. 

### 6.1.7 Minutes of the 7th Project Meeting {#6.1.7-minutes-of-the-7th-project-meeting}

Date:        Oct 28, 2025  
Time:       4:00 pm  
Place:       Room 4368  
Present:    Josh, Gordon, Om  
Absent:    None  
Recorder: Gordon

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   2.1  Gordon has benchmarked the performance difference between different data structures for the matching engine.  
   2.2  All members have completed the first draft of their components, including client SDK, gateway and matching engine.  
   2.3  Client SDK and gateway have been demonstrated to be integrated well together.  
3. Discussion items  
   3.1  Gordon shared his benchmark results, which showed that std::vector was indeed better in terms of operation latency.  
   3.2  Gordon suggested that we backlog replacing node-based data structure with std::vector for the matching engine since the priority should be completing the implementation and integration test. Josh and Om agreed with that.  
   3.3  Josh shared the integration test result. Order requests can be successfully sent from client SDK to the gateway.  
   3.4  Om suggested adding a new component named order manager to our architecture, and removing the historical data handler and the backtester, since it is no longer one of the focuses. Josh and Gordon agreed.  
4. Goals for the coming week  
   4.1  Josh will start implementing the market data processor component.  
   4.2  Gordon will start working on the order manager.  
   4.3  Om will start building our internal websocket client and server for different service use.  
5. Meeting adjournment  
   The meeting was adjourned at 4:45 pm. 

### 6.1.8 Minutes of the 8th Project Meeting {#6.1.8-minutes-of-the-8th-project-meeting}

Date:        Nov 15, 2025  
Time:       11:00 am  
Place:       Room 4368  
Present:    Josh, Gordon, Om  
Absent:    None  
Recorder: Josh

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   2.1  Gordon has implemented the handler of order manager which handles all incoming order requests from gateways.  
   2.2  Josh has implemented a multiple-producer-single-comsumer (MPSC) lock-free ring buffer backed by shared memory, for fast data sharing between processes.  
   2.3  Om has integrated the third-party library websocketpp into the project for our internal websocket client and server use.  
3. Discussion items  
   3.1  Josh shared his design choice for the market data processor using the MPSC lock-free ring buffer.  
   3.2  Gordon questioned if the ring buffer really helped with performance improvement.  
   3.3  Om recalled the difference between lock-free and wait-free, and suggested we verify if the ring buffer is actually lock-free. Josh agreed with that.  
   3.4  Josh pointed out that clearer error messages should be provided by our websocket library.   
4. Goals for the coming week  
   4.1  Josh will continue the development of the market data processor.  
   4.2  Gordon will continue implementing the order manager.  
   4.3  Om will verify the performance improvement of the ring buffer and if it is truly lock-free.  
5. Meeting adjournment  
   The meeting was adjourned at 12:30 pm. 

### 6.1.9 Minutes of the 9th Project Meeting {#6.1.9-minutes-of-the-9th-project-meeting}

Date:        Dec 5, 2025  
Time:       4:30 pm  
Place:       Room 3504  
Present:    Gordon, Om, Prof. Sander  
Absent:    Josh  
Recorder: Gordon

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   Om reported the current progress to Prof. Sander, including the client SDK, gateways, order manager and market data processor.  
3. Discussion items  
   3.1  Gordon demonstrated that an order request sent by the client SDK can be received and recognised correctly by the destination gateway.  
   3.2  Prof. Sander thought the work was great.  
4. Goals for the coming week  
   4.1  All members will continue the development of their components.  
   4.2  Josh will initiate the development of a simple frontend to visualize the LOB and trade data.  
5. Meeting adjournment

The meeting was adjourned at 4:45 pm. 

### 6.1.10 Minutes of the 10th Project Meeting {#6.1.10-minutes-of-the-10th-project-meeting}

Date:        Jan 10, 2026  
Time:       3:00 pm  
Place:       Room 4368  
Present:    Josh, Gordon, Om  
Absent:    None  
Recorder: Josh

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   2.1  Josh finished the implementation of frontend using React and TypeScript. Besides, the market data processor has been completed.  
   2.2  Om refined the internal websocket library to have better error messages for easier debugging. The library now can handle multiple connections.  
   2.3  Gordon has finished the first draft of order manager.  
3. Discussion items  
   3.1  Josh demonstrated that data from the matching engine can now be visualised on the frontend dashboard.  
   3.2  All members agreed that integration tests should be conveyed to ensure all services can work well together.  
4. Goals for the coming week  
   4.1  Gordon will develop the balance checking feature for order manager.  
   4.2  All members will work on integration tests.  
5. Meeting adjournment  
   The meeting was adjourned at 5:00 pm. 

### 6.1.11 Minutes of the 11th Project Meeting {#6.1.11-minutes-of-the-11th-project-meeting}

Date:        Feb 5, 2026  
Time:       3:30 pm  
Place:       Room 3504  
Present:    Josh, Gordon, Om, Prof. Sander  
Absent:    None  
Recorder: Om

1. Approval of minutes  
   The minutes of the last meeting were approved without amendment.  
2. Report on progress  
   2.1  Josh has implemented a generator to generate mock market data to the matching engine. The events happening in the matching engine will be sent to the market data processor via shared memory, then forwarded to clients and frontend dashboard.  
   2.2  Om has enhanced the gateway implementation. Now the system can support multiple gateways to receive requests from clients, and load balancing has been implemented.  
   2.3  Gordon has finished the integration between gateway and order manager.  
   2.4  The team is now working on integrating all components and making sure the data flows correctly and seamlessly.  
3. Discussion items  
   3.1  Gordon explained some technical challenges faced during the past few weeks, including CMake compilation errors and low latency tactics.  
   3.2  Prof. Sander suggests that we implement our own memory allocator to optimize the standard library performance for our use case.  
   3.3  Josh demonstrated the data flow in the system with the frontend dashboard.  
4. Goals for the coming week  
   4.1  All members will continue work on integration tests.  
   4.2  All members will benchmark and optimize the performance of the components of which they are in charge.  
5. Meeting adjournment  
   The meeting was adjourned at 3:50 pm. 

## 6.2 Explanation on Limit Order Books {#6.2-explanation-on-limit-order-books}

Electronic markets are governed by centralized limit order books (LOBs), where heterogeneous agents submit, cancel, and execute orders that collectively form price, liquidity, and volatility dynamics. Each limit order book contains a buy side (also called bids) and a sell side (also called asks). If the bid price is greater than or equal to the ask price, the quantity matched will be removed from the LOB. Orders are first sorted by price, then sorted by the earliest time of arrival. This is known as *price-time priority*. 

For example, suppose we start with an empty LOB. A trader submits a buy order for one share of APPL at $100. Since there is no one selling the desired share, this order sits at the “top” of the LOB as the best bid. When another trader submits a buy order of one share of APPL at $99, this bid sits *behind* the $100 bid due to price-time priority. If another trader then submits a sell order for one APPL share at $100, there is now a *match* between bid and ask prices, and those orders are filled. All that remains on the LOB is the bid at $99, becoming the new best bid. If a seller places an ask for $105, it is clear that no matching is available. This is known as a *bid-ask spread* of $5. The above example describes limit orders. Another popular type of orders are market orders, which match with the best prices immediately.

This simple mechanism allows for market participants to provide and extract information regarding an asset. For example, a larger bid-ask spread is correlated with lower liquidity, reflecting a riskier trade when entering a position. By aggregating buyer and seller intentions in a central LOB, traders are able to infer the fair price of an asset.   


[image1]: <data:image/png;base64,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gwsK6S4EQRxRzZGhmSg/ngMn/jZt2uiiijr33HN1UeIkKRgnqhZ8n+lzggvu8Ap/2sLJN0rUiGY9evTQxzVlMWnSJF2UKF4CAkwMxZNka6+9ti4iqjvoukefDFywWpDu0oV5aBKksbqQqAZk7sCMMvISECRlMnToUF1UNUlto3XWWcdftmyZLiaqK3vssQeO0UIvYiXCSl1QgHOCtLouJKoB3yxatEgf55TF9ttv71999dW6uGqCz8889JAkuAdt/vz5urhq0EZE9Q6jv3iOBm/v64IinKELiGqIPt4ph65du1b1oQEXPi/8t7/xxhvr4oqYMGGCaSNedSNy9543IsqfPu6pEffdd5/5cpQRFcpp9913N3/LtRvxd9hhB3NV7vvvv9dVsTvssMPMCBbShxxRPVuyZAmDN6I68lO2nv4pN9zEH7Sfv+WWW+qqoixcuFC+fP2//OUvutpJ8n4GDhyoq4qCvqzQFxzW+fXXX+tqorp11VVX4bjoY323E1GdwAM/+juBioAht8444wzzkyu6+kC72gkd1qKj32uvvbYiV/KSCB0bDxgwwO/evbu5WqfbCCMuoGPkO++801+8eLF+ORGlpI6ZG1Pf40RUx5ZnG66IiIiSoVWrVgjcvtJf4EREmwbJDINERETVh9s18L0cpI3U93VN2U4XEFFJ/uepn7mYmJiYmCqSnvPqiO7D7fdeeHNf99T8JVbdVUE6KUjrpOYxbirKoKdX45EuERERURLck8qPTOWzUzmi2Fz5/FS+JJWLL9Q8EREREcXMvuwIN6fyr1M5yr9LJZmHSancDuC+98JRGYiIiIioTLaxpvGzabMgnRCkjb1wjFOQgO0BNS8BnORitJonIiIiojJbkcoHpJUSERERUWLJFbYFaaVERERERERERERERERERERERERERERERERERERERERERERURf8f6a8r6MOS5nUAAAAASUVORK5CYII=>

[image2]: <data:image/png;base64,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>

[image3]: <data:image/png;base64,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>