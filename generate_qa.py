# generate_qa.py
"""
Generate 1000+ Q&A pairs for BBDITM and general questions
"""

import json
import random

# Categories and templates
CATEGORIES = {
    "general": {
        "questions": [
            "what is your name", "who made you", "how are you",
            "what can you do", "tell me about yourself", "what is your purpose",
            "are you a robot", "can you see me", "can you hear me",
            "can you talk", "say hello", "hello", "hi", "hey",
            "good morning", "good evening", "good night",
            "thanks", "thank you", "nice", "cool", "awesome", "ok"
        ],
        "answers": [
            "My name is Trishul! I am the Security Robot V2.",
            "I was created by Trishivra Robotics team.",
            "I'm great, bro! Ready to patrol!",
            "I patrol, detect threats, and alert you on Telegram.",
            "I am Trishul, an autonomous security robot.",
            "My purpose is to keep this area safe."
        ]
    },
    "bbditm": {
        "questions": [
            "what is bbditm", "where is bbditm",
            "what is the full form of bbditm", "when was bbditm established",
            "what courses does bbditm offer", "what is the fee for btech at bbditm",
            "what is the placement of bbditm", "is bbditm good for cse",
            "what is the official website of bbditm", "what is the bbditm college code",
            "what is the address of bbditm", "does bbditm have hostel",
            "does bbditm have library", "does bbditm have sports facilities"
        ],
        "answers": [
            "BBDITM is Babu Banarasi Das Institute of Technology and Management in Lucknow.",
            "BBDITM is at BBD City, Ayodhya Road, Lucknow.",
            "BBDITM stands for Babu Banarasi Das Institute of Technology and Management.",
            "BBDITM was established in 1998.",
            "BBDITM offers B.Tech, M.Tech, MBA, B.Arch, and M.Pharm."
        ]
    }
}

def generate_qa_pairs():
    """Generate Q&A pairs by combining templates"""
    qa_pairs = []
    
    # Add basic pairs
    for category, data in CATEGORIES.items():
        for q in data["questions"]:
            for a in data["answers"]:
                qa_pairs.append({
                    "question": q,
                    "answer": a
                })
    
    # Add variations
    variations = [
        ("what is", "tell me about"),
        ("where is", "where can i find"),
        ("what are", "tell me the"),
        ("does", "is there"),
        ("can", "are you able to"),
        ("how", "what is the way to")
    ]
    
    return qa_pairs

if __name__ == "__main__":
    pairs = generate_qa_pairs()
    print(f"Generated {len(pairs)} Q&A pairs")