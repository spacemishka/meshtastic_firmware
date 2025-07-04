#!/usr/bin/env python3

import argparse
import json
import os
import sys
import subprocess
import time
import datetime
import shutil
from pathlib import Path
from typing import Dict, List, Optional

class TestRunner:
    def __init__(self, config_path: str):
        self.config = self.load_config(config_path)
        self.output_dir = Path(self.config['general']['outputDirectory'])
        self.temp_dir = Path(self.config['general']['tempDirectory'])
        self.setup_directories()

    @staticmethod
    def load_config(path: str) -> Dict:
        with open(path, 'r') as f:
            return json.load(f)

    def setup_directories(self):
        """Create necessary directories."""
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.temp_dir.mkdir(parents=True, exist_ok=True)
        
        for subdir in ['logs', 'reports', 'metrics']:
            (self.output_dir / subdir).mkdir(exist_ok=True)

    def run_tests(self, categories: Optional[List[str]] = None) -> bool:
        """Run tests for specified categories or all if none specified."""
        success = True
        start_time = time.time()

        # Filter categories if specified
        test_categories = self.config['categories']
        if categories:
            test_categories = {k: v for k, v in test_categories.items() 
                             if k in categories}

        results = {
            'startTime': datetime.datetime.now().isoformat(),
            'categories': {},
            'summary': {
                'total': 0,
                'passed': 0,
                'failed': 0,
                'skipped': 0
            }
        }

        # Run tests for each category
        for category, settings in test_categories.items():
            if not settings.get('enabled', True):
                print(f"Skipping disabled category: {category}")
                continue

            print(f"\nRunning {category} tests...")
            category_result = self.run_category(category, settings)
            results['categories'][category] = category_result
            
            # Update summary
            results['summary']['total'] += category_result['total']
            results['summary']['passed'] += category_result['passed']
            results['summary']['failed'] += category_result['failed']
            results['summary']['skipped'] += category_result['skipped']
            
            if category_result['failed'] > 0 and self.config['general']['abortOnFailure']:
                print(f"Aborting due to failures in {category}")
                success = False
                break

        # Calculate duration
        duration = time.time() - start_time
        results['duration'] = duration
        results['endTime'] = datetime.datetime.now().isoformat()

        # Generate reports
        if self.config['general']['generateReports']:
            self.generate_reports(results)

        # Cleanup if configured
        if self.config['cleanup']['enabled']:
            self.cleanup()

        return success

    def run_category(self, category: str, settings: Dict) -> Dict:
        """Run tests for a specific category."""
        result = {
            'total': 0,
            'passed': 0,
            'failed': 0,
            'skipped': 0,
            'duration': 0,
            'tests': []
        }

        cmake_target = f"test_{category}"
        start_time = time.time()

        try:
            # Build test target
            subprocess.run(["cmake", "--build", ".", "--target", cmake_target],
                         check=True, capture_output=True)

            # Run tests with timeout
            timeout = settings.get('timeout', 300)
            env = os.environ.copy()
            env['TEST_CATEGORY'] = category
            
            process = subprocess.run(
                [f"./{cmake_target}"],
                timeout=timeout,
                capture_output=True,
                env=env
            )

            # Parse test output
            test_results = self.parse_test_output(process.stdout.decode())
            result.update(test_results)

        except subprocess.TimeoutExpired:
            print(f"Tests in category {category} timed out after {timeout}s")
            result['failed'] += 1
        except subprocess.CalledProcessError as e:
            print(f"Error running tests in category {category}: {e}")
            result['failed'] += 1
        except Exception as e:
            print(f"Unexpected error in category {category}: {e}")
            result['failed'] += 1

        result['duration'] = time.time() - start_time
        return result

    def parse_test_output(self, output: str) -> Dict:
        """Parse test output and extract results."""
        result = {
            'total': 0,
            'passed': 0,
            'failed': 0,
            'skipped': 0,
            'tests': []
        }

        for line in output.splitlines():
            if '[==========]' in line:
                # Extract total tests
                if 'tests from' in line:
                    try:
                        result['total'] = int(line.split()[1])
                    except (IndexError, ValueError):
                        pass
            elif '[ PASSED ]' in line:
                result['passed'] += 1
            elif '[ FAILED ]' in line:
                result['failed'] += 1
            elif '[ SKIPPED ]' in line:
                result['skipped'] += 1

        return result

    def generate_reports(self, results: Dict):
        """Generate test reports in configured formats."""
        timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        
        for format in self.config['reporting']['formats']:
            if format == 'json':
                self.save_json_report(results, timestamp)
            elif format == 'html':
                self.generate_html_report(results, timestamp)
            elif format == 'csv':
                self.generate_csv_report(results, timestamp)

    def save_json_report(self, results: Dict, timestamp: str):
        """Save results as JSON."""
        report_path = self.output_dir / 'reports' / f'report_{timestamp}.json'
        with open(report_path, 'w') as f:
            json.dump(results, f, indent=2)

    def generate_html_report(self, results: Dict, timestamp: str):
        """Generate HTML report using template."""
        template_path = Path(__file__).parent / 'report_template.html'
        if not template_path.exists():
            print("Warning: HTML template not found")
            return

        report_path = self.output_dir / 'reports' / f'report_{timestamp}.html'
        
        with open(template_path, 'r') as f:
            template = f.read()

        # Replace placeholders with actual data
        report = template.replace(
            '"{{RESULTS}}"',
            json.dumps(results, indent=2)
        )

        with open(report_path, 'w') as f:
            f.write(report)

    def generate_csv_report(self, results: Dict, timestamp: str):
        """Generate CSV report."""
        report_path = self.output_dir / 'reports' / f'report_{timestamp}.csv'
        
        with open(report_path, 'w') as f:
            # Write header
            f.write("Category,Total,Passed,Failed,Skipped,Duration\n")
            
            # Write category results
            for category, data in results['categories'].items():
                f.write(f"{category},{data['total']},{data['passed']},"
                       f"{data['failed']},{data['skipped']},{data['duration']:.2f}\n")

    def cleanup(self):
        """Clean up temporary files and old reports."""
        # Clean temp directory
        if self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)
            self.temp_dir.mkdir()

        # Clean old reports based on retention policy
        retention = self.config['reporting']['retention']
        for report_type, days in retention.items():
            self.cleanup_old_files(
                self.output_dir / 'reports',
                f'*_{report_type}.*',
                days
            )

    def cleanup_old_files(self, directory: Path, pattern: str, days: int):
        """Remove files older than specified days."""
        now = time.time()
        for file in directory.glob(pattern):
            if os.path.getmtime(file) < now - (days * 86400):
                file.unlink()

def main():
    parser = argparse.ArgumentParser(description='Run Meshtastic tests')
    parser.add_argument('--config', default='test_config.json',
                       help='Path to test configuration file')
    parser.add_argument('--categories', nargs='+',
                       help='Test categories to run (default: all)')
    args = parser.parse_args()

    try:
        runner = TestRunner(args.config)
        success = runner.run_tests(args.categories)
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Error running tests: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()