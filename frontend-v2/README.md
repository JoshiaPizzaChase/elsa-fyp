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

   HTTP API calls use this backend target, and WebSocket (`mdpEndpoint`) is now composed in frontend from backend-provided `mdp_ip` + `mdp_port` (`/active_servers` / `/server_mdp_endpoint`) instead of hardcoded frontend values.
