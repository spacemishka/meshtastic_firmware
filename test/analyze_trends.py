#!/usr/bin/env python3

import argparse
import json
import os
import sys
from pathlib import Path
import datetime
import numpy as np
import matplotlib.pyplot as plt
from typing import Dict, List, Tuple
from dataclasses import dataclass

@dataclass
class TestMetric:
    timestamp: datetime.datetime
    duration: float
    memory: int
    success: bool

@dataclass
class TrendAnalysis:
    mean: float
    std_dev: float
    trend: float  # Slope of linear regression
    anomalies: List[datetime.datetime]
    prediction: float

class TrendAnalyzer:
    def __init__(self, report_dir: str):
        self.report_dir = Path(report_dir)
        self.metrics_by_test: Dict[str, List[TestMetric]] = {}
        self.load_reports()

    def load_reports(self):
        """Load all JSON test reports from the report directory."""
        for report_file in sorted(self.report_dir.glob('report_*.json')):
            try:
                with open(report_file, 'r') as f:
                    report = json.load(f)
                    self.process_report(report)
            except Exception as e:
                print(f"Error loading {report_file}: {e}")

    def process_report(self, report: Dict):
        """Extract metrics from a test report."""
        timestamp = datetime.datetime.fromisoformat(report['startTime'])
        
        for category in report.get('categories', {}).values():
            for test in category.get('tests', []):
                test_name = test['name']
                metric = TestMetric(
                    timestamp=timestamp,
                    duration=test.get('duration', 0),
                    memory=test.get('memoryUsage', 0),
                    success=test.get('passed', False)
                )
                
                if test_name not in self.metrics_by_test:
                    self.metrics_by_test[test_name] = []
                self.metrics_by_test[test_name].append(metric)

    def analyze_trends(self) -> Dict[str, Dict[str, TrendAnalysis]]:
        """Analyze trends for each test and metric type."""
        results = {}
        
        for test_name, metrics in self.metrics_by_test.items():
            results[test_name] = {
                'duration': self.analyze_metric([m.duration for m in metrics]),
                'memory': self.analyze_metric([m.memory for m in metrics]),
                'success_rate': self.analyze_success_rate(metrics)
            }
        
        return results

    def analyze_metric(self, values: List[float]) -> TrendAnalysis:
        """Analyze a specific metric's trend."""
        if not values:
            return TrendAnalysis(0, 0, 0, [], 0)

        values = np.array(values)
        x = np.arange(len(values))

        # Calculate basic statistics
        mean = np.mean(values)
        std_dev = np.std(values)

        # Calculate trend (linear regression)
        trend = np.polyfit(x, values, 1)[0] if len(values) > 1 else 0

        # Detect anomalies (values outside 2 standard deviations)
        anomalies = []
        for i, value in enumerate(values):
            if abs(value - mean) > 2 * std_dev:
                anomalies.append(self.get_timestamp_for_index(i))

        # Predict next value
        prediction = trend * len(values) + (np.polyfit(x, values, 1)[1] if len(values) > 1 else mean)

        return TrendAnalysis(mean, std_dev, trend, anomalies, prediction)

    def analyze_success_rate(self, metrics: List[TestMetric]) -> TrendAnalysis:
        """Analyze success rate trends."""
        if not metrics:
            return TrendAnalysis(0, 0, 0, [], 0)

        # Calculate success rates over time
        window_size = 5
        rates = []
        
        for i in range(len(metrics) - window_size + 1):
            window = metrics[i:i + window_size]
            success_rate = sum(1 for m in window if m.success) / len(window)
            rates.append(success_rate)

        return self.analyze_metric(rates)

    def get_timestamp_for_index(self, index: int) -> datetime.datetime:
        """Get timestamp for a specific index in the metrics."""
        timestamps = sorted(set(m.timestamp for metrics in self.metrics_by_test.values() 
                              for m in metrics))
        return timestamps[index] if index < len(timestamps) else timestamps[-1]

    def plot_trends(self, output_dir: str):
        """Generate trend plots."""
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)

        for test_name, metrics in self.metrics_by_test.items():
            self.plot_test_metrics(test_name, metrics, output_path)

    def plot_test_metrics(self, test_name: str, metrics: List[TestMetric], output_dir: Path):
        """Generate plots for a specific test's metrics."""
        timestamps = [m.timestamp for m in metrics]
        durations = [m.duration for m in metrics]
        memories = [m.memory / (1024 * 1024) for m in metrics]  # Convert to MB
        success_rates = self.calculate_success_rates(metrics)

        plt.figure(figsize=(15, 10))

        # Duration plot
        plt.subplot(3, 1, 1)
        plt.plot(timestamps, durations, 'b-', label='Duration')
        plt.title(f'{test_name} - Performance Trends')
        plt.ylabel('Duration (s)')
        plt.grid(True)
        plt.legend()

        # Memory plot
        plt.subplot(3, 1, 2)
        plt.plot(timestamps, memories, 'g-', label='Memory Usage')
        plt.ylabel('Memory (MB)')
        plt.grid(True)
        plt.legend()

        # Success rate plot
        plt.subplot(3, 1, 3)
        plt.plot(timestamps[4:], success_rates, 'r-', label='Success Rate')
        plt.ylabel('Success Rate')
        plt.xlabel('Time')
        plt.grid(True)
        plt.legend()

        plt.tight_layout()
        plt.savefig(output_dir / f'{test_name}_trends.png')
        plt.close()

    def calculate_success_rates(self, metrics: List[TestMetric]) -> List[float]:
        """Calculate rolling success rates."""
        window_size = 5
        rates = []
        
        for i in range(len(metrics) - window_size + 1):
            window = metrics[i:i + window_size]
            success_rate = sum(1 for m in window if m.success) / len(window)
            rates.append(success_rate)
            
        return rates

    def generate_report(self, output_file: str):
        """Generate a detailed trend analysis report."""
        trends = self.analyze_trends()
        
        with open(output_file, 'w') as f:
            f.write("Test Performance Trend Analysis\n")
            f.write("============================\n\n")

            for test_name, analysis in trends.items():
                f.write(f"\n{test_name}\n")
                f.write("-" * len(test_name) + "\n\n")

                # Duration trends
                f.write("Duration Analysis:\n")
                self.write_metric_analysis(f, analysis['duration'])

                # Memory trends
                f.write("\nMemory Usage Analysis:\n")
                self.write_metric_analysis(f, analysis['memory'])

                # Success rate trends
                f.write("\nSuccess Rate Analysis:\n")
                self.write_metric_analysis(f, analysis['success_rate'])

                f.write("\n" + "=" * 50 + "\n")

    def write_metric_analysis(self, f, analysis: TrendAnalysis):
        """Write analysis details to the report."""
        f.write(f"  Mean: {analysis.mean:.2f}\n")
        f.write(f"  Standard Deviation: {analysis.std_dev:.2f}\n")
        f.write(f"  Trend: {'Increasing' if analysis.trend > 0 else 'Decreasing'} "
                f"({abs(analysis.trend):.2f} per test)\n")
        
        if analysis.anomalies:
            f.write("  Anomalies detected at:\n")
            for timestamp in analysis.anomalies:
                f.write(f"    - {timestamp}\n")
        
        f.write(f"  Next predicted value: {analysis.prediction:.2f}\n")

def main():
    parser = argparse.ArgumentParser(description='Analyze test performance trends')
    parser.add_argument('--report-dir', required=True,
                       help='Directory containing test reports')
    parser.add_argument('--output-dir', required=True,
                       help='Directory for output files')
    args = parser.parse_args()

    try:
        analyzer = TrendAnalyzer(args.report_dir)
        
        # Generate plots
        analyzer.plot_trends(args.output_dir)
        
        # Generate report
        analyzer.generate_report(os.path.join(args.output_dir, 'trend_analysis.txt'))
        
        print(f"Analysis completed. Results saved to {args.output_dir}")
        
    except Exception as e:
        print(f"Error analyzing trends: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()