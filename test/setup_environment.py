#!/usr/bin/env python3

import argparse
import os
import sys
import subprocess
import platform
import shutil
import venv
import json
from pathlib import Path
from typing import Dict, List, Optional

class TestEnvironment:
    def __init__(self, base_dir: str):
        self.base_dir = Path(base_dir)
        self.venv_dir = self.base_dir / "venv"
        self.config_file = self.base_dir / "test_config.json"
        self.requirements_file = self.base_dir / "requirements.txt"

    def setup(self, force: bool = False) -> bool:
        """Set up the test environment."""
        try:
            print("Setting up test environment...")
            
            # Create directories
            self._create_directories()

            # Create virtual environment
            if force or not self.venv_dir.exists():
                self._create_virtual_environment()

            # Install dependencies
            self._install_dependencies()

            # Configure environment
            self._configure_environment()

            print("Environment setup completed successfully")
            return True

        except Exception as e:
            print(f"Error setting up environment: {e}", file=sys.stderr)
            return False

    def cleanup(self) -> bool:
        """Clean up test environment."""
        try:
            print("Cleaning up test environment...")

            # Remove virtual environment
            if self.venv_dir.exists():
                shutil.rmtree(self.venv_dir)

            # Clean temporary files
            self._clean_temp_files()

            print("Environment cleanup completed successfully")
            return True

        except Exception as e:
            print(f"Error cleaning up environment: {e}", file=sys.stderr)
            return False

    def validate(self) -> bool:
        """Validate test environment setup."""
        try:
            print("Validating test environment...")

            # Check virtual environment
            if not self._check_virtual_environment():
                return False

            # Verify dependencies
            if not self._verify_dependencies():
                return False

            # Validate configuration
            if not self._validate_configuration():
                return False

            print("Environment validation successful")
            return True

        except Exception as e:
            print(f"Error validating environment: {e}", file=sys.stderr)
            return False

    def _create_directories(self):
        """Create necessary directories."""
        dirs = [
            self.base_dir / "logs",
            self.base_dir / "reports",
            self.base_dir / "temp",
            self.base_dir / "data"
        ]

        for dir_path in dirs:
            dir_path.mkdir(parents=True, exist_ok=True)

    def _create_virtual_environment(self):
        """Create Python virtual environment."""
        print("Creating virtual environment...")
        venv.create(self.venv_dir, with_pip=True)

    def _install_dependencies(self):
        """Install required dependencies."""
        print("Installing dependencies...")
        
        pip_cmd = self._get_pip_command()
        
        # Upgrade pip
        subprocess.run([pip_cmd, "install", "--upgrade", "pip"], check=True)

        # Install requirements
        if self.requirements_file.exists():
            subprocess.run([pip_cmd, "install", "-r", str(self.requirements_file)], check=True)

    def _configure_environment(self):
        """Configure test environment."""
        # Load configuration
        if self.config_file.exists():
            with open(self.config_file, 'r') as f:
                config = json.load(f)
        else:
            config = self._create_default_config()
            with open(self.config_file, 'w') as f:
                json.dump(config, f, indent=2)

        # Set environment variables
        os.environ['TEST_CONFIG_FILE'] = str(self.config_file)
        os.environ['TEST_OUTPUT_DIR'] = str(self.base_dir / "reports")
        os.environ['TEST_DATA_DIR'] = str(self.base_dir / "data")
        os.environ['TEST_TEMP_DIR'] = str(self.base_dir / "temp")

    def _clean_temp_files(self):
        """Clean temporary files and directories."""
        patterns = [
            "*.pyc",
            "__pycache__",
            "*.log",
            "*.tmp"
        ]

        for pattern in patterns:
            for file in self.base_dir.rglob(pattern):
                if file.is_file():
                    file.unlink()
                elif file.is_dir():
                    shutil.rmtree(file)

    def _check_virtual_environment(self) -> bool:
        """Check virtual environment setup."""
        if not self.venv_dir.exists():
            print("Virtual environment not found", file=sys.stderr)
            return False

        pip_cmd = self._get_pip_command()
        try:
            subprocess.run([pip_cmd, "--version"], check=True, capture_output=True)
            return True
        except subprocess.CalledProcessError:
            print("Failed to verify pip installation", file=sys.stderr)
            return False

    def _verify_dependencies(self) -> bool:
        """Verify installed dependencies."""
        pip_cmd = self._get_pip_command()
        
        try:
            # Get installed packages
            result = subprocess.run(
                [pip_cmd, "freeze"],
                check=True,
                capture_output=True,
                text=True
            )
            installed = result.stdout.splitlines()

            # Check against requirements
            with open(self.requirements_file, 'r') as f:
                required = f.readlines()

            missing = []
            for req in required:
                req = req.strip()
                if req and not req.startswith('#'):
                    if not any(r.lower().startswith(req.lower().split('>=')[0]) 
                             for r in installed):
                        missing.append(req)

            if missing:
                print("Missing dependencies:", file=sys.stderr)
                for dep in missing:
                    print(f"  {dep}", file=sys.stderr)
                return False

            return True

        except Exception as e:
            print(f"Error verifying dependencies: {e}", file=sys.stderr)
            return False

    def _validate_configuration(self) -> bool:
        """Validate configuration file."""
        try:
            with open(self.config_file, 'r') as f:
                config = json.load(f)

            # Check required sections
            required_sections = ['general', 'logging', 'metrics', 'reporting']
            for section in required_sections:
                if section not in config:
                    print(f"Missing required section: {section}", file=sys.stderr)
                    return False

            return True

        except Exception as e:
            print(f"Error validating configuration: {e}", file=sys.stderr)
            return False

    def _get_pip_command(self) -> str:
        """Get the appropriate pip command for the virtual environment."""
        if platform.system() == "Windows":
            return str(self.venv_dir / "Scripts" / "pip.exe")
        return str(self.venv_dir / "bin" / "pip")

    def _create_default_config(self) -> Dict:
        """Create default configuration."""
        return {
            "general": {
                "outputDirectory": "reports",
                "tempDirectory": "temp",
                "enableParallelExecution": True,
                "maxThreads": 4
            },
            "logging": {
                "enabled": True,
                "minLevel": "INFO",
                "maxFileSize": 10485760
            },
            "metrics": {
                "enabled": True,
                "collectMemoryMetrics": True,
                "collectTimingMetrics": True
            },
            "reporting": {
                "formats": ["html", "json", "csv"],
                "generateCharts": True
            }
        }

def main():
    parser = argparse.ArgumentParser(description='Test Environment Manager')
    parser.add_argument('--base-dir', default='.',
                       help='Base directory for test environment')
    parser.add_argument('--action', choices=['setup', 'cleanup', 'validate'],
                       required=True, help='Action to perform')
    parser.add_argument('--force', action='store_true',
                       help='Force setup even if environment exists')
    args = parser.parse_args()

    env = TestEnvironment(args.base_dir)
    
    if args.action == 'setup':
        success = env.setup(args.force)
    elif args.action == 'cleanup':
        success = env.cleanup()
    else:  # validate
        success = env.validate()

    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()