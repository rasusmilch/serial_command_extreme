Import("env")
import os
import subprocess

def generate_coverage_report(*args, **kwargs):
    """Generate code coverage report after tests"""
    print("Generating code coverage report...")
    
    # Run lcov to capture coverage data
    build_dir = os.path.join(env.subst("$PROJECT_DIR"), ".pio", "build", "native")
    coverage_dir = os.path.join(env.subst("$PROJECT_DIR"), "coverage")
    
    # Create coverage directory
    if not os.path.exists(coverage_dir):
        os.makedirs(coverage_dir)
    
    # Generate initial coverage info
    try:
        subprocess.run([
            "lcov",
            "--capture",
            "--directory", build_dir,
            "--output-file", os.path.join(coverage_dir, "coverage.info"),
            "--rc", "lcov_branch_coverage=1"
        ], check=True)
        
        # Filter out system headers and test files
        subprocess.run([
            "lcov",
            "--remove", os.path.join(coverage_dir, "coverage.info"),
            "/usr/*", "*/test/*", "*/.pio/*", "*/ArduinoFake/*",
            "--output-file", os.path.join(coverage_dir, "coverage_filtered.info"),
            "--rc", "lcov_branch_coverage=1"
        ], check=True)
        
        # Generate HTML report
        subprocess.run([
            "genhtml",
            os.path.join(coverage_dir, "coverage_filtered.info"),
            "--output-directory", os.path.join(coverage_dir, "html"),
            "--branch-coverage",
            "--title", "SerialCommandManager Code Coverage"
        ], check=True)
        
        print(f"Coverage report generated at: {os.path.join(coverage_dir, 'html', 'index.html')}")
        
    except FileNotFoundError:
        print("WARNING: lcov/genhtml not found. Install with: choco install lcov (Windows) or apt-get install lcov (Linux)")
    except subprocess.CalledProcessError as e:
        print(f"WARNING: Coverage generation failed: {e}")

# Hook to run after tests
env.AddPostAction("test", generate_coverage_report)