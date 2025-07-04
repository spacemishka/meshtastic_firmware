// Queue Visualization Module
const QueueVisualizer = {
    // Chart objects
    charts: {
        queueStatus: null,
        priorityDist: null,
        timeline: null,
        performance: null
    },

    // Configuration
    config: {
        refreshRate: 1000,
        maxDataPoints: 100,
        animations: true,
        theme: 'light'
    },

    init() {
        this.setupUI();
        this.initCharts();
        this.bindEvents();
        this.startUpdates();
    },

    setupUI() {
        const visualizerDiv = document.createElement('div');
        visualizerDiv.innerHTML = `
            <div class="card mt-4">
                <div class="card-header d-flex justify-content-between align-items-center">
                    <h5 class="card-title mb-0">Queue Visualization</h5>
                    <div class="btn-group">
                        <button id="toggleTheme" class="btn btn-sm btn-outline-secondary">
                            <i class="fas fa-adjust"></i>
                        </button>
                        <button id="toggleAnimations" class="btn btn-sm btn-outline-secondary">
                            <i class="fas fa-film"></i>
                        </button>
                        <button id="downloadChart" class="btn btn-sm btn-outline-primary">
                            <i class="fas fa-download"></i>
                        </button>
                    </div>
                </div>
                <div class="card-body">
                    <div class="row">
                        <div class="col-md-6">
                            <div class="chart-container">
                                <canvas id="queueStatusChart"></canvas>
                            </div>
                        </div>
                        <div class="col-md-6">
                            <div class="chart-container">
                                <canvas id="priorityDistChart"></canvas>
                            </div>
                        </div>
                    </div>
                    <div class="row mt-4">
                        <div class="col-12">
                            <div class="chart-container">
                                <canvas id="timelineChart"></canvas>
                            </div>
                        </div>
                    </div>
                    <div class="row mt-4">
                        <div class="col-12">
                            <div class="chart-container">
                                <canvas id="performanceChart"></canvas>
                            </div>
                        </div>
                    </div>
                </div>
            </div>`;

        document.querySelector('#queueMonitor').appendChild(visualizerDiv);
    },

    initCharts() {
        const chartDefaults = {
            responsive: true,
            animation: this.config.animations,
            plugins: {
                legend: {
                    position: 'top',
                },
                tooltip: {
                    mode: 'index',
                    intersect: false
                }
            }
        };

        // Queue Status Chart
        this.charts.queueStatus = new Chart('queueStatusChart', {
            type: 'bar',
            data: {
                labels: ['Current', 'Average', 'Peak'],
                datasets: [{
                    label: 'Queue Size',
                    data: [0, 0, 0],
                    backgroundColor: ['#36a2eb', '#ffcd56', '#ff6384']
                }]
            },
            options: {
                ...chartDefaults,
                scales: {
                    y: {
                        beginAtZero: true,
                        max: config.lora.window_queue_size
                    }
                }
            }
        });

        // Priority Distribution Chart
        this.charts.priorityDist = new Chart('priorityDistChart', {
            type: 'doughnut',
            data: {
                labels: ['High Priority', 'Normal Priority', 'Queued'],
                datasets: [{
                    data: [0, 0, 0],
                    backgroundColor: ['#ff6384', '#36a2eb', '#ffcd56']
                }]
            },
            options: chartDefaults
        });

        // Timeline Chart
        this.charts.timeline = new Chart('timelineChart', {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Queue Size',
                        data: [],
                        borderColor: '#36a2eb',
                        fill: false
                    },
                    {
                        label: 'Drop Rate',
                        data: [],
                        borderColor: '#ff6384',
                        fill: false
                    }
                ]
            },
            options: {
                ...chartDefaults,
                scales: {
                    x: {
                        type: 'time',
                        time: {
                            unit: 'minute',
                            displayFormats: {
                                minute: 'HH:mm'
                            }
                        }
                    },
                    y: {
                        beginAtZero: true
                    }
                }
            }
        });

        // Performance Chart
        this.charts.performance = new Chart('performanceChart', {
            type: 'radar',
            data: {
                labels: [
                    'Queue Efficiency',
                    'Processing Speed',
                    'Memory Usage',
                    'Priority Balance',
                    'Drop Rate'
                ],
                datasets: [{
                    label: 'Current',
                    data: [0, 0, 0, 0, 0],
                    backgroundColor: 'rgba(54, 162, 235, 0.2)',
                    borderColor: '#36a2eb'
                }]
            },
            options: {
                ...chartDefaults,
                scales: {
                    r: {
                        beginAtZero: true,
                        max: 100
                    }
                }
            }
        });
    },

    bindEvents() {
        document.getElementById('toggleTheme').addEventListener('click', () => {
            this.config.theme = this.config.theme === 'light' ? 'dark' : 'light';
            this.updateTheme();
        });

        document.getElementById('toggleAnimations').addEventListener('click', () => {
            this.config.animations = !this.config.animations;
            this.updateAnimations();
        });

        document.getElementById('downloadChart').addEventListener('click', () => {
            this.downloadCharts();
        });
    },

    startUpdates() {
        setInterval(() => this.updateCharts(), this.config.refreshRate);
    },

    async updateCharts() {
        try {
            const status = await window.moduleConfig.getTimeWindowStatus();
            this.updateQueueStatus(status);
            this.updatePriorityDist(status);
            this.updateTimeline(status);
            this.updatePerformance(status);
        } catch (err) {
            console.error('Failed to update charts:', err);
        }
    },

    updateTheme() {
        Chart.defaults.color = this.config.theme === 'dark' ? '#fff' : '#666';
        this.updateCharts();
    },

    updateAnimations() {
        Object.values(this.charts).forEach(chart => {
            chart.options.animation = this.config.animations;
            chart.update();
        });
    },

    downloadCharts() {
        const zip = new JSZip();
        Object.entries(this.charts).forEach(([name, chart]) => {
            const dataUrl = chart.toBase64Image();
            const data = atob(dataUrl.split(',')[1]);
            zip.file(`${name}.png`, data, {binary: true});
        });
        
        zip.generateAsync({type: 'blob'}).then(blob => {
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'queue-charts.zip';
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        });
    }
};

// Initialize when document is ready
document.addEventListener('DOMContentLoaded', () => {
    QueueVisualizer.init();
});