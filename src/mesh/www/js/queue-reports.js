// Queue Automated Reporting Module
const QueueReports = {
    // Report configuration
    config: {
        enabled: false,
        interval: 3600, // 1 hour
        retention: 7,   // days
        types: {
            stats: true,
            alerts: true,
            performance: true
        },
        format: 'csv'
    },

    // Report storage
    reports: [],
    
    init() {
        this.loadConfig();
        this.setupUI();
        this.bindEvents();
        this.startReporting();
    },

    loadConfig() {
        const saved = localStorage.getItem('queueReportConfig');
        if (saved) {
            this.config = {...this.config, ...JSON.parse(saved)};
        }
    },

    setupUI() {
        const reportsDiv = document.createElement('div');
        reportsDiv.innerHTML = `
            <div class="card mt-4">
                <div class="card-header d-flex justify-content-between align-items-center">
                    <h5 class="card-title mb-0">Automated Reports</h5>
                    <div class="form-check form-switch">
                        <input class="form-check-input" type="checkbox" id="enableReports">
                        <label class="form-check-label">Enable</label>
                    </div>
                </div>
                <div class="card-body">
                    <!-- Report Configuration -->
                    <form id="reportConfig">
                        <div class="form-group">
                            <label>Report Interval</label>
                            <select class="form-control" id="reportInterval">
                                <option value="900">15 Minutes</option>
                                <option value="3600">1 Hour</option>
                                <option value="14400">4 Hours</option>
                                <option value="86400">Daily</option>
                            </select>
                        </div>
                        
                        <div class="form-group">
                            <label>Data Retention (days)</label>
                            <input type="number" class="form-control" id="reportRetention"
                                   min="1" max="30" value="7">
                        </div>
                        
                        <div class="form-group">
                            <label>Report Types</label>
                            <div class="form-check">
                                <input class="form-check-input" type="checkbox" id="reportStats">
                                <label class="form-check-label">Queue Statistics</label>
                            </div>
                            <div class="form-check">
                                <input class="form-check-input" type="checkbox" id="reportAlerts">
                                <label class="form-check-label">Alert History</label>
                            </div>
                            <div class="form-check">
                                <input class="form-check-input" type="checkbox" id="reportPerformance">
                                <label class="form-check-label">Performance Metrics</label>
                            </div>
                        </div>
                        
                        <div class="form-group">
                            <label>Export Format</label>
                            <select class="form-control" id="reportFormat">
                                <option value="csv">CSV</option>
                                <option value="json">JSON</option>
                            </select>
                        </div>
                        
                        <button type="submit" class="btn btn-primary">Save Configuration</button>
                    </form>

                    <!-- Generated Reports -->
                    <div class="mt-4">
                        <h6>Generated Reports</h6>
                        <div class="table-responsive">
                            <table class="table table-sm">
                                <thead>
                                    <tr>
                                        <th>Date</th>
                                        <th>Type</th>
                                        <th>Size</th>
                                        <th>Actions</th>
                                    </tr>
                                </thead>
                                <tbody id="reportsList"></tbody>
                            </table>
                        </div>
                    </div>
                </div>
            </div>`;

        document.querySelector('#queueMonitor').appendChild(reportsDiv);
    },

    bindEvents() {
        document.getElementById('enableReports').checked = this.config.enabled;
        document.getElementById('reportInterval').value = this.config.interval;
        document.getElementById('reportRetention').value = this.config.retention;
        document.getElementById('reportStats').checked = this.config.types.stats;
        document.getElementById('reportAlerts').checked = this.config.types.alerts;
        document.getElementById('reportPerformance').checked = this.config.types.performance;
        document.getElementById('reportFormat').value = this.config.format;

        document.getElementById('enableReports').addEventListener('change', (e) => {
            this.config.enabled = e.target.checked;
            this.saveConfig();
            if (e.target.checked) {
                this.startReporting();
            }
        });

        document.getElementById('reportConfig').addEventListener('submit', (e) => {
            e.preventDefault();
            this.saveConfig();
        });
    },

    saveConfig() {
        this.config = {
            enabled: document.getElementById('enableReports').checked,
            interval: parseInt(document.getElementById('reportInterval').value),
            retention: parseInt(document.getElementById('reportRetention').value),
            types: {
                stats: document.getElementById('reportStats').checked,
                alerts: document.getElementById('reportAlerts').checked,
                performance: document.getElementById('reportPerformance').checked
            },
            format: document.getElementById('reportFormat').value
        };

        localStorage.setItem('queueReportConfig', JSON.stringify(this.config));
        this.startReporting();
    },

    startReporting() {
        if (this.reportTimer) {
            clearInterval(this.reportTimer);
        }

        if (this.config.enabled) {
            this.reportTimer = setInterval(() => {
                this.generateReport();
            }, this.config.interval * 1000);
        }
    },

    async generateReport() {
        const report = {
            timestamp: new Date(),
            data: {}
        };

        if (this.config.types.stats) {
            report.data.stats = await this.collectStats();
        }

        if (this.config.types.alerts) {
            report.data.alerts = QueueAlerts.history;
        }

        if (this.config.types.performance) {
            report.data.performance = await this.collectPerformance();
        }

        this.saveReport(report);
        this.cleanOldReports();
        this.updateReportsList();
    },

    async collectStats() {
        return {
            queueSize: QueueMonitor.history.queueSize,
            highPriority: QueueMonitor.history.highPriority,
            normalPriority: QueueMonitor.history.normalPriority,
            dropped: QueueMonitor.history.dropped
        };
    },

    async collectPerformance() {
        const status = await window.moduleConfig.getTimeWindowStatus();
        return {
            avgQueueTime: status.metrics.avg_queue_time,
            maxQueueTime: status.metrics.max_queue_time,
            queueOverflows: status.metrics.queue_overflows,
            processingEfficiency: this.calculateEfficiency(status)
        };
    },

    calculateEfficiency(status) {
        const processed = status.metrics.high_priority_count + 
                        status.metrics.normal_priority_count;
        const total = processed + status.metrics.drop_count;
        return total ? (processed / total) * 100 : 100;
    },

    saveReport(report) {
        this.reports.push(report);
        localStorage.setItem('queueReports', JSON.stringify(this.reports));
    },

    cleanOldReports() {
        const cutoff = new Date();
        cutoff.setDate(cutoff.getDate() - this.config.retention);
        
        this.reports = this.reports.filter(report => 
            new Date(report.timestamp) > cutoff
        );
        
        localStorage.setItem('queueReports', JSON.stringify(this.reports));
    },

    updateReportsList() {
        const tbody = document.getElementById('reportsList');
        tbody.innerHTML = this.reports.map(report => `
            <tr>
                <td>${new Date(report.timestamp).toLocaleString()}</td>
                <td>${Object.keys(report.data).join(', ')}</td>
                <td>${this.getReportSize(report)}KB</td>
                <td>
                    <button class="btn btn-sm btn-outline-primary" 
                            onclick="QueueReports.downloadReport('${report.timestamp}')">
                        <i class="fas fa-download"></i>
                    </button>
                    <button class="btn btn-sm btn-outline-danger"
                            onclick="QueueReports.deleteReport('${report.timestamp}')">
                        <i class="fas fa-trash"></i>
                    </button>
                </td>
            </tr>
        `).join('');
    },

    getReportSize(report) {
        return Math.round(JSON.stringify(report).length / 1024);
    },

    downloadReport(timestamp) {
        const report = this.reports.find(r => r.timestamp === timestamp);
        if (!report) return;

        if (this.config.format === 'csv') {
            QueueExport.downloadCSV(
                this.convertToCSV(report),
                `queue-report-${new Date(timestamp).toISOString()}.csv`
            );
        } else {
            const blob = new Blob([JSON.stringify(report, null, 2)], 
                                {type: 'application/json'});
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `queue-report-${new Date(timestamp).toISOString()}.json`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }
    },

    deleteReport(timestamp) {
        this.reports = this.reports.filter(r => r.timestamp !== timestamp);
        localStorage.setItem('queueReports', JSON.stringify(this.reports));
        this.updateReportsList();
    },

    convertToCSV(report) {
        // Implementation similar to QueueExport.convertToCSV
        // but adapted for report format
    }
};

// Initialize when document is ready
document.addEventListener('DOMContentLoaded', () => {
    QueueReports.init();
});