# frontend-v2

Real-time trading dashboard with K-line chart, order book, and latest trades.

## Setup

1. Install dependencies:
   ```bash
   cd frontend-v2
   npm install
   ```

2. Configure backend HTTP API target in `.env`:
   ```env
   REACT_APP_BACKEND_IP=localhost
   REACT_APP_BACKEND_PORT=8080
   ```

   This only affects HTTP API calls. WebSocket (`mdpEndpoint`) remains unchanged.
