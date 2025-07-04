// Queue Alert System
const QueueAlerts = {
    // Alert thresholds
    thresholds: {
        queueFull: 0.9,          // 90% full
        highDropRate: 0.1,       // 10% drop rate
        longQueueTime: 300,      // 5 minutes
        priorityImbalance: 0.8   // 80% one priority type
    },

    // Alert states
    alerts: new Set(),
    
    // Alert history
    history: [],
    maxHistory: 50,

    init() {
        this.loadThresholds();
        this.setupUI();
        this.startMonitoring();
    },

    loadThresholds() {
        // Load saved thresholds from localStorage
        const saved = localStorage.getItem('queueAlertThresholds');
        if (saved) {
            this.thresholds = {...this.thresholds, ...JSON.parse(saved)};
        }
    },

    setupUI() {
        const alertsDiv = document.createElement('div');
        alertsDiv.innerHTML = `
            <div class="card mt-4">
                <div class="card-header d-flex justify-content-between align-items-center">
                    <h5 class="card-title mb-0">Queue Alerts</h5>
                    <div class="btn-group">
                        <button id="configureAlerts" class="btn btn-sm btn-outline-secondary">
                            <i class="fas fa-cog"></i>
                        </button>
                        <button id="clearAlerts" class="btn btn-sm btn-outline-danger">
                            <i class="fas fa-trash"></i>
                        </button>
                    </div>
                </div>
                <div class="card-body">
                    <div id="activeAlerts"></div>
                    <div id="alertHistory" class="mt-3 small"></div>
                </div>
            </div>
            
            <!-- Alert Configuration Modal -->
            <div class="modal fade" id="alertConfigModal">
                <div class="modal-dialog">
                    <div class="modal-content">
                        <div class="modal-header">
                            <h5 class="modal-title">Alert Configuration</h5>
                            <button type="button" class="close" data-dismiss="modal">&times;</button>
                        </div>
                        <div class="modal-body">
                            <form id="alertConfigForm">
                                <div class="form-group">
                                    <label>Queue Full Threshold (%)</label>
                                    <input type="number" class="form-control" name="queueFull" 
                                           min="0" max="100" step="5">
                                </div>
                                <div class="form-group">
                                    <label>High Drop Rate (%)</label>
                                    <input type="number" class="form-control" name="highDropRate"
                                           min="0" max="100" step="1">
                                </div>
                                <div class="form-group">
                                    <label>Long Queue Time (seconds)</label>
                                    <input type="number" class="form-control" name="longQueueTime"
                                           min="0" step="60">
                                </div>
                                <div class="form-group">
                                    <label>Priority Imbalance Threshold (%)</label>
                                    <input type="number" class="form-control" name="priorityImbalance"
                                           min="0" max="100" step="5">
                                </div>
                            </form>
                        </div>
                        <div class="modal-footer">
                            <button type="button" class="btn btn-secondary" data-dismiss="modal">Cancel</button>
                            <button type="button" class="btn btn-primary" id="saveAlertConfig">Save</button>
                        </div>
                    </div>
                </div>
            </div>`;

        document.querySelector('#queueMonitor').appendChild(alertsDiv);
        this.bindEvents();
    },

    bindEvents() {
        document.getElementById('configureAlerts').addEventListener('click', () => {
            this.showConfigModal();
        });

        document.getElementById('clearAlerts').addEventListener('click', () => {
            this.clearAlerts();
        });

        document.getElementById('saveAlertConfig').addEventListener('click', () => {
            this.saveConfiguration();
        });
    },

    startMonitoring() {
        // Check for alerts every 5 seconds
        setInterval(() => this.checkAlerts(), 5000);
    },

    checkAlerts(stats) {
        const newAlerts = new Set();

        // Queue fullness alert
        const queuePercent = stats.queue_size / stats.max_queue_size;
        if (queuePercent >= this.thresholds.queueFull) {
            newAlerts.add({
                type: 'queueFull',
                severity: 'danger',
                message: `Queue ${Math.round(queuePercent * 100)}% full`
            });
        }

        // Drop rate alert
        const dropRate = stats.metrics.drop_count / 
            (stats.metrics.total_processed || 1);
        if (dropRate >= this.thresholds.highDropRate) {
            newAlerts.add({
                type: 'highDropRate',
                severity: 'warning',
                message: `High drop rate: ${Math.round(dropRate * 100)}%`
            });
        }

        // Queue time alert
        if (stats.metrics.avg_queue_time >= this.thresholds.longQueueTime) {
            newAlerts.add({
                type: 'longQueueTime',
                severity: 'warning',
                message: `Long queue time: ${Math.round(stats.metrics.avg_queue_time)}s`
            });
        }

        // Priority imbalance alert
        const totalPriority = stats.metrics.high_priority_count + 
                            stats.metrics.normal_priority_count;
        if (totalPriority > 0) {
            const highPriorityRatio = stats.metrics.high_priority_count / totalPriority;
            if (highPriorityRatio >= this.thresholds.priorityImbalance) {
                newAlerts.add({
                    type: 'priorityImbalance',
                    severity: 'info',
                    message: `High priority packets dominating: ${Math.round(highPriorityRatio * 100)}%`
                });
            }
        }

        this.updateAlerts(newAlerts);
    },

    updateAlerts(newAlerts) {
        // Check for new alerts
        for (const alert of newAlerts) {
            if (!this.alerts.has(alert.type)) {
                this.addAlert(alert);
            }
        }

        // Check for cleared alerts
        for (const oldAlert of this.alerts) {
            if (!newAlerts.has(oldAlert.type)) {
                this.removeAlert(oldAlert.type);
            }
        }

        this.alerts = newAlerts;
        this.updateUI();
    },

    addAlert(alert) {
        alert.timestamp = new Date();
        this.history.unshift(alert);
        if (this.history.length > this.maxHistory) {
            this.history.pop();
        }
        this.showNotification(alert);
    },

    removeAlert(alertType) {
        const alert = {
            type: alertType,
            timestamp: new Date(),
            cleared: true
        };
        this.history.unshift(alert);
        if (this.history.length > this.maxHistory) {
            this.history.pop();
        }
    },

    updateUI() {
        // Update active alerts
        const activeAlertsHtml = Array.from(this.alerts)
            .map(alert => `
                <div class="alert alert-${alert.severity} mb-2">
                    ${alert.message}
                </div>
            `).join('');
        document.getElementById('activeAlerts').innerHTML = activeAlertsHtml;

        // Update alert history
        const historyHtml = this.history
            .map(alert => `
                <div class="text-${alert.cleared ? 'muted' : alert.severity}">
                    ${alert.timestamp.toLocaleTimeString()} - 
                    ${alert.cleared ? 'Cleared: ' : ''}${alert.message}
                </div>
            `).join('');
        document.getElementById('alertHistory').innerHTML = historyHtml;
    },

    showNotification(alert) {
        if ('Notification' in window && Notification.permission === 'granted') {
            new Notification('Queue Alert', {
                body: alert.message,
                icon: '/icon.png'
            });
        }
    },

    clearAlerts() {
        this.alerts.clear();
        this.updateUI();
    },

    saveConfiguration() {
        const form = document.getElementById('alertConfigForm');
        const formData = new FormData(form);
        
        for (const [key, value] of formData.entries()) {
            this.thresholds[key] = parseFloat(value) / 100;
        }
        
        localStorage.setItem('queueAlertThresholds', 
            JSON.stringify(this.thresholds));
        
        $('#alertConfigModal').modal('hide');
    },

    showConfigModal() {
        const form = document.getElementById('alertConfigForm');
        for (const [key, value] of Object.entries(this.thresholds)) {
            const input = form.elements[key];
            if (input) {
                input.value = Math.round(value * 100);
            }
        }
        $('#alertConfigModal').modal('show');
    }
};

// Initialize when document is ready
document.addEventListener('DOMContentLoaded', () => {
    QueueAlerts.init();
});