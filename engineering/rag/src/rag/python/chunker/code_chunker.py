"""
代码切分模块 — Tree-sitter 语义切分

使用 Tree-sitter 解析代码 AST，提取函数、类、文档字符串等语义单元。
支持 Python/JavaScript/C/C++/Go/Rust/Java 多语言。
"""

import logging
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)


class CodeChunk:
    """代码块输出"""
    def __init__(
        self,
        content: str = "",
        language: str = "",
        name: Optional[str] = None,
        docstring: Optional[str] = None,
        start_line: int = 0,
        end_line: int = 0,
        metadata: dict = None,
    ):
        self.content = content
        self.language = language
        self.name = name
        self.docstring = docstring
        self.start_line = start_line
        self.end_line = end_line
        self.metadata = metadata or {}


class CodeChunker:
    """
    Tree-sitter 语义代码切分器

    支持按函数/类定义切分代码，提取文档字符串，
    多语言支持（Python/JavaScript/C/C++/Go/Rust/Java）。

    用法:
        chunker = CodeChunker()
        chunks = chunker.chunk(code_content, "example.py", {"source": "test"})
    """

    LANGUAGE_EXTENSIONS = {
        "py": "python",
        "js": "javascript",
        "ts": "typescript",
        "c": "c",
        "cpp": "cpp",
        "h": "c",
        "hpp": "cpp",
        "go": "go",
        "rs": "rust",
        "java": "java",
        "rb": "ruby",
        "php": "php",
        "swift": "swift",
        "kt": "kotlin",
    }

    FUNCTION_TYPES = {
        "python": "function_definition",
        "javascript": "function_declaration",
        "typescript": "function_declaration",
        "c": "function_definition",
        "cpp": "function_definition",
        "go": "function_declaration",
        "rust": "function_item",
        "java": "method_declaration",
    }

    def __init__(self, max_chunk_lines: int = 200):
        self.max_chunk_lines = max_chunk_lines

    def chunk(
        self,
        code_content: str,
        file_path: str,
        metadata: dict = None,
    ) -> list[CodeChunk]:
        """
        执行代码切分

        Args:
            code_content: 代码文本
            file_path: 文件路径（用于检测语言）
            metadata: 附加元数据

        Returns:
            CodeChunk 列表
        """
        if metadata is None:
            metadata = {}

        # 1. 检测语言
        ext = Path(file_path).suffix.lstrip(".")
        lang_name = self._detect_language(ext)

        if lang_name == "plaintext":
            logger.info(f"非代码文件，跳过: {file_path}")
            return []

        # 2. 尝试用 Tree-sitter 解析
        chunks = self._parse_with_treesitter(code_content, lang_name, file_path)

        # 3. 如果没有识别到语义单元，整体作为一个 chunk
        if not chunks:
            chunks.append(CodeChunk(
                content=code_content,
                language=lang_name,
                name=Path(file_path).stem,
                docstring=None,
                start_line=1,
                end_line=len(code_content.splitlines()),
                metadata=metadata,
            ))

        # 4. 为每个 chunk 注入 metadata
        for chunk in chunks:
            chunk.metadata = {**metadata, **chunk.metadata}

        return chunks

    def _parse_with_treesitter(
        self,
        code_content: str,
        lang_name: str,
        file_path: str,
    ) -> list[CodeChunk]:
        """
        使用 Tree-sitter 解析代码

        Args:
            code_content: 代码文本
            lang_name: 语言名称
            file_path: 文件路径

        Returns:
            CodeChunk 列表
        """
        try:
            import tree_sitter_languages
            from tree_sitter import Parser

            lang = tree_sitter_languages.get_language(lang_name)
            parser = Parser(lang)
            tree = parser.parse(code_content.encode())

            chunks = []
            chunks_by_line = {}

            def traverse(node):
                node_type = node.type

                # 函数定义
                func_type = self.FUNCTION_TYPES.get(lang_name)
                if func_type and node_type == func_type:
                    chunk = self._extract_function_node(code_content, node, lang_name)
                    if chunk:
                        chunks.append(chunk)
                        chunks_by_line[node.start_point[0]] = chunk

                # 类定义
                elif node_type == "class_definition":
                    chunk = self._extract_class_node(code_content, node, lang_name)
                    if chunk:
                        chunks.append(chunk)

                # 递归遍历
                for child in node.children:
                    traverse(child)

            traverse(tree.root_node)
            return chunks

        except ImportError:
            logger.warning("tree-sitter-languages 未安装，跳过 AST 解析")
            return []
        except Exception as e:
            logger.warning(f"Tree-sitter 解析失败: {e}")
            return []

    def _extract_function_node(
        self,
        code: str,
        node,
        lang: str,
    ) -> Optional[CodeChunk]:
        """
        提取单个函数节点

        Args:
            code: 完整代码文本
            node: Tree-sitter 节点
            lang: 语言名称

        Returns:
            CodeChunk 对象，失败返回 None
        """
        func_code = code[node.start_byte:node.end_byte]

        # 检查行数限制
        lines = func_code.splitlines()
        if len(lines) > self.max_chunk_lines:
            logger.debug(f"函数过长，跳过: {len(lines)} 行")

        # 提取函数名
        name = self._extract_name(node, lang)

        # 提取文档字符串
        docstring = self._extract_docstring(code, node, lang)

        return CodeChunk(
            content=func_code,
            language=lang,
            name=name,
            docstring=docstring,
            start_line=node.start_point[0] + 1,
            end_line=node.end_point[0] + 1,
            metadata={"type": "function"},
        )

    def _extract_class_node(
        self,
        code: str,
        node,
        lang: str,
    ) -> Optional[CodeChunk]:
        """
        提取类定义节点

        Args:
            code: 完整代码文本
            node: Tree-sitter 节点
            lang: 语言名称

        Returns:
            CodeChunk 对象
        """
        class_code = code[node.start_byte:node.end_byte]

        # 提取类名
        name = self._extract_name(node, lang)

        # 提取文档字符串
        docstring = self._extract_docstring(code, node, lang)

        return CodeChunk(
            content=class_code,
            language=lang,
            name=name,
            docstring=docstring,
            start_line=node.start_point[0] + 1,
            end_line=node.end_point[0] + 1,
            metadata={"type": "class"},
        )

    @staticmethod
    def _extract_name(node, lang: str) -> Optional[str]:
        """
        从节点中提取名称

        Args:
            node: Tree-sitter 节点
            lang: 语言名称

        Returns:
            名称字符串
        """
        # 在子节点中查找 name 字段
        for child in node.children:
            if child.type == "name":
                return child.text.decode("utf-8") if isinstance(child.text, bytes) else child.text
            # Python 的 function_definition 有 name 子节点
            if child.type == "identifier":
                return child.text.decode("utf-8") if isinstance(child.text, bytes) else child.text
        return None

    @staticmethod
    def _extract_docstring(code: str, node, lang: str) -> Optional[str]:
        """
        提取函数/类的文档字符串

        Args:
            code: 完整代码文本
            node: Tree-sitter 节点
            lang: 语言名称

        Returns:
            文档字符串，不存在返回 None
        """
        try:
            for child in node.children:
                # Python 的 docstring 在 body 的第一个 expression_statement 中
                if child.type == "body":
                    for body_child in child.children:
                        if body_child.type in ("expression_statement", "string"):
                            return code[body_child.start_byte:body_child.end_byte]
                # 其他语言的字符串字面量
                if child.type in ("string", "string_literal", "comment"):
                    return code[child.start_byte:child.end_byte]
        except Exception:
            pass
        return None

    @staticmethod
    def _detect_language(ext: str) -> str:
        """
        根据文件扩展名检测语言

        Args:
            ext: 文件扩展名（不含点号）

        Returns:
            语言名称，"plaintext" 表示不支持
        """
        return CodeChunker.LANGUAGE_EXTENSIONS.get(ext, "plaintext")