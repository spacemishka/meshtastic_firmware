// Queue Data Export Module
const QueueExport = {
    init() {
        this.setupUI();
        this.bindEvents();
    },

    setupUI() {
        const exportDiv = document.createElement('div');
        exportDiv.innerHTML = `
            <div class="card mt-4">
                <div class="card-header d-flex justify-content-between align-items-center">
                    <h5 class="card-title mb-0">Export Data</h5>
                    <div class="btn-group">
                        <button id="exportStats" class="btn btn-sm btn-outline-primary">
                            <i class="fas fa-download"></i> Statistics
                        </button>
                        <button id="exportAlerts" class="btn btn-sm btn-outline-warning">
                            <i class="fas fa-bell"></i> Alert History
                        </button>
                        <button id="exportQueue" class="btn btn-sm btn-outline-success">
                            <i class="fas fa-list"></i> Queue Data
                        </button>
                    </div>
                </div>
                <div class="card-body">
                    <form id="exportForm" class="form-inline">
                        <div class="form-group mr-2">
                            <label class="mr-2">Time Range:</label>
                            <select class="form-control form-control-sm" id="exportRange">
                                <option value="hour">Last Hour</option>
                                <option value="day">Last 24 Hours</option>
                                <option value="week">Last Week</option>
                                <option value="all">All Data</option>
                            </select>
                        </div>
                        <div class="form-check mr-2">
                            <input class="form-check-input" type="checkbox" id="includeTimestamps">
                            <label class="form-check-label">Include Timestamps</label>
                        </div>
                    </form>
                </div>
            </div>`;

        document.querySelector('#queueMonitor').appendChild(exportDiv);
    },

    bindEvents() {
        document.getElementById('exportStats').addEventListener('click', () => {
            this.exportStatistics();
        });

        document.getElementById('exportAlerts').addEventListener('click', () => {
            this.exportAlertHistory();
        });

        document.getElementById('exportQueue').addEventListener('click', () => {
            this.exportQueueData();
        });
    },

    async exportStatistics() {
        const range = document.getElementById('exportRange').value;
        const includeTime = document.getElementById('includeTimestamps').checked;
        
        try {
            const stats = await this.collectStatistics(range);
            const csvData = this.convertToCSV(stats, {
                headers: [
                    'Timestamp',
                    'Queue Size',
                    'High Priority',
                    'Normal Priority',
                    'Dropped',
                    'Avg Queue Time',
                    'Max Queue Time'
                ],
                includeTimestamps: includeTime
            });
            
            this.downloadCSV(csvData, `queue-stats-${range}.csv`);
        } catch (err) {
            console.error('Failed to export statistics:', err);
            alert('Failed to export statistics');
        }
    },

    async exportAlertHistory() {
        const alerts = QueueAlerts.history;
        const csvData = this.convertToCSV(alerts, {
            headers: ['Timestamp', 'Type', 'Severity', 'Message', 'Cleared'],
            transform: alert => ({
                timestamp: alert.timestamp.toISOString(),
                type: alert.type,
                severity: alert.severity || 'info',
                message: alert.message,
                cleared: alert.cleared ? 'Yes' : 'No'
            })
        });
        
        this.downloadCSV(csvData, 'queue-alerts.csv');
    },

    async exportQueueData() {
        const queueData = await this.getQueueSnapshot();
        const csvData = this.convertToCSV(queueData, {
            headers: [
                'Position',
                'Packet ID',
                'Priority',
                'Queue Time',
                'Size',
                'Type'
            ]
        });
        
        this.downloadCSV(csvData, 'queue-snapshot.csv');
    },

    async collectStatistics(range) {
        const now = Date.now();
        let startTime;
        
        switch (range) {
            case 'hour':
                startTime = now - (60 * 60 * 1000);
                break;
            case 'day':
                startTime = now - (24 * 60 * 60 * 1000);
                break;
            case 'week':
                startTime = now - (7 * 24 * 60 * 60 * 1000);
                break;
            default:
                startTime = 0;
        }
        
        return QueueMonitor.history.filter(entry => 
            entry.timestamp >= startTime
        );
    },

    async getQueueSnapshot() {
        // Get current queue contents
        const status = await window.moduleConfig.getTimeWindowStatus();
        return status.queue.map((packet, index) => ({
            position: index + 1,
            packetId: packet.id,
            priority: packet.priority,
            queueTime: Math.round((Date.now() - packet.enqueue_time) / 1000),
            size: packet.size,
            type: packet.type
        }));
    },

    convertToCSV(data, options) {
        const { headers, transform, includeTimestamps } = options;
        const rows = [];
        
        // Add headers
        rows.push(headers.join(','));
        
        // Add data rows
        data.forEach(item => {
            const row = [];
            const processedItem = transform ? transform(item) : item;
            
            headers.forEach(header => {
                let value = processedItem[header.toLowerCase().replace(/\s+/g, '_')];
                
                // Format timestamps
                if (header === 'Timestamp' && !includeTimestamps) {
                    value = new Date(value).toLocaleString();
                }
                
                // Handle special characters
                if (typeof value === 'string' && value.includes(',')) {
                    value = `"${value}"`;
                }
                
                row.push(value ?? '');
            });
            
            rows.push(row.join(','));
        });
        
        return rows.join('\n');
    },

    downloadCSV(csvData, filename) {
        const blob = new Blob([csvData], { type: 'text/csv;charset=utf-8;' });
        const link = document.createElement('a');
        
        if (navigator.msSaveBlob) {
            navigator.msSaveBlob(blob, filename);
            return;
        }
        
        link.href = URL.createObjectURL(blob);
        link.setAttribute('download', filename);
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    }
};

// Initialize when document is ready
document.addEventListener('DOMContentLoaded', () => {
    QueueExport.init();
});