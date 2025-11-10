#!/bin/bash
# Monitor memory usage of ooc_omp_farm process

echo "Monitoring memory usage every 2 seconds..."
echo "Press Ctrl+C to stop"
echo ""
echo "Time      | RSS (MB) | VSZ (MB) | %MEM | Description"
echo "----------|----------|----------|------|------------"

while true; do
    # Find ooc_omp_farm process
    PID=$(pgrep -f "ooc_omp_farm" | head -1)
    
    if [ -z "$PID" ]; then
        echo "$(date +%H:%M:%S) | Process not running"
    else
        # Get memory stats
        STATS=$(ps -p $PID -o rss=,vsz=,%mem= 2>/dev/null)
        
        if [ -n "$STATS" ]; then
            RSS_KB=$(echo $STATS | awk '{print $1}')
            VSZ_KB=$(echo $STATS | awk '{print $2}')
            MEM_PCT=$(echo $STATS | awk '{print $3}')
            
            RSS_MB=$((RSS_KB / 1024))
            VSZ_MB=$((VSZ_KB / 1024))
            
            # Determine phase based on memory
            if [ $RSS_MB -lt 1000 ]; then
                PHASE="Starting up"
            elif [ $RSS_MB -lt 3000 ]; then
                PHASE="Reading/Sorting"
            elif [ $RSS_MB -lt 5000 ]; then
                PHASE="Collecting (queue bounded)"
            else
                PHASE="⚠️  High memory!"
            fi
            
            printf "%s | %8d | %8d | %4.1f | %s\n" \
                "$(date +%H:%M:%S)" "$RSS_MB" "$VSZ_MB" "$MEM_PCT" "$PHASE"
        fi
    fi
    
    sleep 2
done
