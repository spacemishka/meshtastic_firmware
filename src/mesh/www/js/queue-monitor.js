// Queue Monitor Module
const QueueMonitor = {
    // Chart objects
    queueChart: null,
    priorityChart: null,
    
    // Update interval (ms)
    updateInterval: 5000,
    
    // Historical data
    history: {
        queueSize: [],
        highPriority: [],
        normalPriority: [],
        dropped: [],
        timestamps: [],
        maxPoints: 100
    },

    init() {
        this.initCharts();
        this.startMonitoring();
        this.bindEvents();
    },

    initCharts() {
        // Queue size chart
        const queueCtx = document.getElementById('queueSizeChart').getContext('2d');
        this.queueChart = new Chart(queueCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Queue Size',
                    data: [],
                    borderColor: 'rgb(75, 192, 192)',
                    tension: 0.1
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: true,
                        suggestedMax: config.lora.window_queue_size
                    }
                }
            }
        });

        // Priority distribution chart
        const priorityCtx = document.getElementById('priorityChart').getContext('2d');
        this.priorityChart = new Chart(priorityCtx, {
            type: 'pie',
            data: {
                labels: ['High Priority', 'Normal Priority', 'Dropped'],
                datasets: [{
                    data: [0, 0, 0],
                    backgroundColor: [
                        'rgb(255, 99, 132)',
                        'rgb(75, 192, 192)',
                        'rgb(201, 203, 207)'
                    ]
                }]
            }
        });
    },

    startMonitoring() {
        this.updateStats();
        setInterval(() => this.updateStats(), this.updateInterval);
    },

    bindEvents() {
        document.getElementById('clearQueue').addEventListener('click', () => {
            window.moduleConfig.sendCommand('window clear');
            this.updateStats();
        });

        document.getElementById('resetStats').addEventListener('click', () => {
            window.moduleConfig.sendCommand('window reset_stats');
            this.resetHistory();
            this.updateStats();
        });
    },

    async updateStats() {
        try {
            const status = await window.moduleConfig.getTimeWindowStatus();
            this.updateHistory(status);
            this.updateCharts();
            this.updateMetrics(status);
        } catch (err) {
            console.error('Failed to update queue stats:', err);
        }
    },

    updateHistory(status) {
        const now = new Date().toLocaleTimeString();
        
        // Add new data points
        this.history.timestamps.push(now);
        this.history.queueSize.push(status.queue_size);
        this.history.highPriority.push(status.metrics.high_priority_count);
        this.history.normalPriority.push(status.metrics.normal_priority_count);
        this.history.dropped.push(status.metrics.drop_count);

        // Limit history size
        if (this.history.timestamps.length > this.history.maxPoints) {
            this.history.timestamps.shift();
            this.history.queueSize.shift();
            this.history.highPriority.shift();
            this.history.normalPriority.shift();
            this.history.dropped.shift();
        }
    },

    updateCharts() {
        // Update queue size chart
        this.queueChart.data.labels = this.history.timestamps;
        this.queueChart.data.datasets[0].data = this.history.queueSize;
        this.queueChart.update();

        // Update priority distribution chart
        const lastIndex = this.history.highPriority.length - 1;
        this.priorityChart.data.datasets[0].data = [
            this.history.highPriority[lastIndex],
            this.history.normalPriority[lastIndex],
            this.history.dropped[lastIndex]
        ];
        this.priorityChart.update();
    },

    updateMetrics(status) {
        document.getElementById('queueMetrics').innerHTML = `
            <div class="row">
                <div class="col">
                    <h6>Queue Status</h6>
                    <p>Current Size: ${status.queue_size} / ${config.lora.window_queue_size}</p>
                    <p>Avg Queue Time: ${Math.round(status.metrics.avg_queue_time)}s</p>
                    <p>Max Queue Time: ${Math.round(status.metrics.max_queue_time)}s</p>
                </div>
                <div class="col">
                    <h6>Packet Metrics</h6>
                    <p>High Priority: ${status.metrics.high_priority_count}</p>
                    <p>Normal Priority: ${status.metrics.normal_priority_count}</p>
                    <p>Dropped: ${status.metrics.drop_count}</p>
                </div>
            </div>
            <div class="progress mt-2" style="height: 20px;">
                <div class="progress-bar bg-success" 
                     role="progressbar" 
                     style="width: ${(status.queue_size / config.lora.window_queue_size) * 100}%">
                    ${status.queue_size} packets
                </div>
            </div>
        `;
    },

    resetHistory() {
        Object.keys(this.history).forEach(key => {
            if (Array.isArray(this.history[key])) {
                this.history[key] = [];
            }
        });
    }
};

// Initialize when document is ready
document.addEventListener('DOMContentLoaded', () => {
    QueueMonitor.init();
});