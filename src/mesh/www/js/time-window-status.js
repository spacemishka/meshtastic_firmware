// Time Window Status Module
const TimeWindowStatus = {
    updateStatus(status) {
        if (!status.lora || !status.lora.time_window_enabled) {
            return '';
        }

        const windowActive = this.isTimeWindowActive(
            status.lora.window_start_hour,
            status.lora.window_start_minute,
            status.lora.window_end_hour,
            status.lora.window_end_minute
        );

        const modeText = {
            0: 'Drop packets',
            1: 'Queue packets',
            2: 'Receive only'
        }[status.lora.window_mode];

        let html = `
            <div class="status-item">
                <i class="fas fa-clock ${windowActive ? 'text-success' : 'text-warning'}"></i>
                <span>Time Window: ${windowActive ? 'Active' : 'Inactive'}</span>
                <small class="text-muted">
                    ${this.formatTime(status.lora.window_start_hour, status.lora.window_start_minute)} - 
                    ${this.formatTime(status.lora.window_end_hour, status.lora.window_end_minute)}
                    (${modeText})
                </small>
            </div>`;

        // Add queue status if in queue mode
        if (status.lora.window_mode === 1) { // QUEUE_PACKETS
            const queueStatus = status.radio && status.radio.queue ? status.radio.queue : { size: 0, max: status.lora.window_queue_size };
            const queuePercent = (queueStatus.size / queueStatus.max) * 100;
            const queueClass = queuePercent > 80 ? 'text-danger' : 
                             queuePercent > 50 ? 'text-warning' : 'text-success';

            html += `
                <div class="status-item">
                    <i class="fas fa-layer-group ${queueClass}"></i>
                    <span>Queue Status: ${queueStatus.size}/${queueStatus.max}</span>
                    <div class="progress" style="height: 4px; width: 100px; display: inline-block; margin-left: 10px;">
                        <div class="progress-bar ${queueClass}" style="width: ${queuePercent}%"></div>
                    </div>
                </div>`;
        }

        return html;
    },

    isTimeWindowActive(startH, startM, endH, endM) {
        const now = new Date();
        const current = now.getHours() * 60 + now.getMinutes();
        const start = startH * 60 + startM;
        const end = endH * 60 + endM;
        
        if (start <= end) {
            return current >= start && current < end;
        } else {
            return current >= start || current < end;
        }
    },

    formatTime(hours, minutes) {
        return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}`;
    },

    // Get next window transition time (open or close)
    getNextTransition(status) {
        if (!status.lora || !status.lora.time_window_enabled) {
            return null;
        }

        const now = new Date();
        const current = now.getHours() * 60 + now.getMinutes();
        const start = status.lora.window_start_hour * 60 + status.lora.window_start_minute;
        const end = status.lora.window_end_hour * 60 + status.lora.window_end_minute;

        let nextTime;
        if (start <= end) {
            if (current < start) {
                nextTime = start;
            } else if (current < end) {
                nextTime = end;
            } else {
                nextTime = start + 24 * 60; // Next day
            }
        } else {
            if (current < end) {
                nextTime = end;
            } else if (current < start) {
                nextTime = start;
            } else {
                nextTime = end + 24 * 60; // Next day
            }
        }

        const transitionDate = new Date(now);
        transitionDate.setHours(Math.floor(nextTime / 60) % 24);
        transitionDate.setMinutes(nextTime % 60);
        if (nextTime >= 24 * 60) {
            transitionDate.setDate(transitionDate.getDate() + 1);
        }

        return transitionDate;
    }
};

// Export for use in other modules
window.TimeWindowStatus = TimeWindowStatus;