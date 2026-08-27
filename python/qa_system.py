#!/usr/bin/env python3
"""
Q&A System for Security Robot V2
Loads questions from JSON and provides intelligent answer matching
"""

import json
import re
import os
import logging
from pathlib import Path
from typing import Optional, Dict, List, Tuple

logger = logging.getLogger(__name__)

# Path to Q&A data
DATA_DIR = Path(__file__).resolve().parents[1] / "data"
QA_FILE = DATA_DIR / "qa_dictionary.json"


class QASystem:
    """
    Q&A system for Trishul Security Robot
    Handles loading, searching, and matching questions
    """
    
    def __init__(self, qa_file_path: str = None):
        self.qa_file_path = qa_file_path or QA_FILE
        self.qa_data = []
        self.qa_map = {}
        self.patterns = []
        self.loaded = False
        
        # Load data
        self.load_qa_data()
    
    def load_qa_data(self) -> bool:
        """Load Q&A data from JSON file"""
        try:
            if not self.qa_file_path.exists():
                logger.error(f"❌ Q&A file not found: {self.qa_file_path}")
                # Create default data
                self.create_default_data()
                return False
            
            with open(self.qa_file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            self.qa_data = data.get('questions', [])
            self.build_search_index()
            self.loaded = True
            
            logger.info(f"✅ Loaded {len(self.qa_data)} questions")
            return True
            
        except Exception as e:
            logger.error(f"❌ Failed to load Q&A: {e}")
            return False
    
    def build_search_index(self):
        """Build search index for fast matching"""
        self.qa_map = {}
        self.patterns = []
        
        for item in self.qa_data:
            q = item.get('question', '').lower().strip()
            a = item.get('answer', '')
            
            # Store exact match
            self.qa_map[q] = a
            
            # Store pattern for fuzzy matching
            # Remove punctuation and extra spaces
            pattern = re.sub(r'[^\w\s]', '', q)
            pattern = re.sub(r'\s+', ' ', pattern)
            self.patterns.append((pattern, a))
    
    def create_default_data(self):
        """Create default Q&A data if file not found"""
        default_data = {
            "questions": [
                {
                    "question": "what is your name",
                    "answer": "My name is Trishul! I am the Security Robot V2."
                },
                {
                    "question": "how are you",
                    "answer": "I'm great, bro! Ready to patrol!"
                },
                {
                    "question": "who made you",
                    "answer": "The Trishivra Robotics team made me."
                }
            ]
        }
        
        try:
            os.makedirs(self.qa_file_path.parent, exist_ok=True)
            with open(self.qa_file_path, 'w', encoding='utf-8') as f:
                json.dump(default_data, f, indent=2)
            logger.info("✅ Default Q&A data created")
        except Exception as e:
            logger.error(f"❌ Failed to create default data: {e}")
    
    def get_answer(self, question: str) -> Tuple[str, float]:
        """
        Get answer for a question with confidence score
        
        Args:
            question: User's question string
            
        Returns:
            Tuple of (answer, confidence)
            confidence: 1.0 = exact match, 0.0 = no match
        """
        if not question:
            return "I didn't hear a question, bro. Try again!", 0.0
        
        q = question.lower().strip()
        
        # 1. Exact match
        if q in self.qa_map:
            return self.qa_map[q], 1.0
        
        # 2. Remove punctuation and extra spaces
        cleaned = re.sub(r'[^\w\s]', '', q)
        cleaned = re.sub(r'\s+', ' ', cleaned)
        
        # 3. Check for partial matches
        best_match = None
        best_score = 0.0
        
        for pattern, answer in self.patterns:
            # Exact pattern match
            if cleaned == pattern:
                return answer, 0.95
            
            # Contains match (user question contains pattern OR pattern contains user question)
            if pattern in cleaned or cleaned in pattern:
                score = min(len(pattern), len(cleaned)) / max(len(pattern), len(cleaned))
                if score > best_score and score > 0.6:
                    best_score = score
                    best_match = answer
        
        # 4. Word-level matching
        if best_score < 0.7:
            words = set(cleaned.split())
            for pattern, answer in self.patterns:
                pattern_words = set(pattern.split())
                common = words & pattern_words
                if common:
                    score = len(common) / max(len(words), len(pattern_words))
                    if score > best_score and score > 0.5:
                        best_score = score
                        best_match = answer
        
        if best_match and best_score > 0.5:
            return best_match, best_score
        
        # 5. No match found
        return self.get_fallback_response(), 0.0
    
    def get_fallback_response(self) -> str:
        """Return a fallback response when no match is found"""
        fallbacks = [
            "I don't know that yet, bro. But I'll learn!",
            "Sorry, I don't have an answer for that. Ask me something else!",
            "Hmm, I haven't been trained on that. Try again!",
            "I'm not sure about that, bro. Maybe later!",
            "That's a new question for me. I'll add it to my memory!",
            "I don't know, bro. But I'm always learning!"
        ]
        import random
        return random.choice(fallbacks)
    
    def add_question(self, question: str, answer: str) -> bool:
        """Add a new question to the database"""
        try:
            q = question.lower().strip()
            if q in self.qa_map:
                return False
            
            new_item = {
                "question": question,
                "answer": answer
            }
            self.qa_data.append(new_item)
            self.qa_map[q] = answer
            
            # Save to file
            with open(self.qa_file_path, 'w', encoding='utf-8') as f:
                json.dump({"questions": self.qa_data}, f, indent=2)
            
            logger.info(f"✅ Added new Q&A: {question}")
            return True
            
        except Exception as e:
            logger.error(f"❌ Failed to add question: {e}")
            return False


# Global instance
qa_system = QASystem()


# ============================================================
# Wrapper Functions
# ============================================================

def get_answer(question: str) -> str:
    """Get answer for a question"""
    answer, confidence = qa_system.get_answer(question)
    return answer

def add_question(question: str, answer: str) -> bool:
    """Add a new question to the Q&A system"""
    return qa_system.add_question(question, answer)

def get_random_greeting() -> str:
    """Get a random greeting"""
    greetings = [
        "Hello, bro! I'm Trishul, your security robot.",
        "Hey there! Trishul here, ready to patrol.",
        "Hi, bro! I'm fully charged and ready to go.",
        "Greetings, bro! I'm Trishul. How can I help?",
        "Namaste, bro! I'm Trishul, your autonomous security robot."
    ]
    import random
    return random.choice(greetings)


# ============================================================
# Test
# ============================================================

if __name__ == "__main__":
    print("🧪 Testing Q&A System...")
    print("=" * 40)
    
    test_questions = [
        "what is your name",
        "how are you",
        "who made you",
        "what is bbditm",
        "where is bbditm",
        "what courses does bbditm offer",
        "hello",
        "what is the weather",
        "good morning",
        "who is trishivra"
    ]
    
    for q in test_questions:
        answer = get_answer(q)
        print(f"Q: {q}")
        print(f"A: {answer}\n")
    
    print("✅ Test complete!")