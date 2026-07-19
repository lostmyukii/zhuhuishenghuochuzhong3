import unittest
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[1]


class DeployContractTests(unittest.TestCase):
    def test_templates_require_https_security_and_safe_cache_boundaries(self):
        nginx = (REPOSITORY / "deploy/nginx-static.conf.example").read_text(encoding="utf-8")
        headers = (REPOSITORY / "deploy/_headers.example").read_text(encoding="utf-8")
        for token in (
            "Content-Security-Policy",
            "Permissions-Policy",
            "serial=(self)",
            "microphone=(self)",
            "X-Content-Type-Options",
            "no-cache, no-store, must-revalidate",
            "public, max-age=31536000, immutable",
        ):
            self.assertIn(token, nginx)
            self.assertIn(token, headers)
        self.assertIn("listen 443 ssl", nginx)
        self.assertIn("map $uri $smartlife_cache_control", nginx)

    def test_repository_does_not_contain_an_automatic_deployment_workflow(self):
        workflow_directory = REPOSITORY / ".github/workflows"
        if workflow_directory.exists():
            workflow_text = "\n".join(
                path.read_text(encoding="utf-8", errors="replace")
                for path in workflow_directory.glob("*.y*ml")
            )
            self.assertNotIn("pages", workflow_text.lower())
            self.assertNotIn("deploy", workflow_text.lower())
        gitignore = (REPOSITORY / ".gitignore").read_text(encoding="utf-8")
        self.assertIn("dist/", gitignore)


if __name__ == "__main__":
    unittest.main()
