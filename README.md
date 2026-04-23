# Market Simulation for Algorithmic Trading

C++ Version: C++23

FIX Version: FIX4.2

# Setup

Note: Please make sure the boost version >= 1.86.0

## C++ Dependencies Installation

```
./scripts/setup.sh
```

## libpqxx installation

1. First ensure you installed any postgresql related libraries on your system, perhaps via the package manager.
   This includes `libpq`, the underlying C-library powering `libpqxx`.
2. It is recommended to install libpqxx from source, and important to disable shared libraries. Without it, you *may*
   encounter double-free issues.

```bash
git clone https://github.com/jtv/libpqxx.git
cd libpqxx
./configure --disable-shared
make
sudo make install
```

## questdb installation

Run this and you should see `deps/c-questdb-client/...` in your project directory:

```bash
mkdir deps
cd deps
git clone https://github.com/questdb/c-questdb-client.git
cd ..
```

## Frontend dependencies installation

```
cd frontend-v2
npm install
```

## Store machine info into database

This distributive system will need multiple machines to deploy different services including gateway, order manager,
matching engine, market data processor, etc. The information of each machine like IP address will be stored in the
postgreSQL database such that backend service can assign machines to different service deployments.

```
psql -d edux_core_db
edux_core_db=# select * from machines;
 machine_id | machine_name |     ip      |          created_ts           |          modified_ts          
------------+--------------+-------------+-------------------------------+-------------------------------
          2 | dev          | 10.89.13.40 | 2026-04-09 22:49:22.91113+08  | 2026-04-09 22:49:22.91113+08
```

## Deployment

On Master node, deploy backend and frontend

```
./scripts/build.sh
sudo ./scripts/deploy_backend.sh 8080
sudo ./scripts/deploy_frontend.sh
```

On each Worker node, deploy a deployment server that receives deployment request from backend service on Master node.

```
./scripts/build.sh
sudo ./scripts/deploy_deployment_server.sh
```

For testing and development purpose, the above can be deployed on a single machine as well.

```
./scripts/build.sh
sudo ./scripts/deploy_backend.sh 8080
sudo ./scripts/deploy_frontend.sh
sudo ./scripts/deploy_deployment_server.sh
```

After that, you can access the frontend via `http://<master node public IP>:3000` where you can create your own market
server.