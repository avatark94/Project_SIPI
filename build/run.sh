#!/bin/bash
export DATABASE_URL="host=localhost dbname=taskplanner user=taskuser password=taskpass"
export JWT_SECRET="local-secret"
./task_planner_web
