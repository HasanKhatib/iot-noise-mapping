from diagrams import Diagram, Cluster, Edge
from diagrams.aws.compute import ECS
from diagrams.aws.network import APIGateway
from diagrams.aws.storage import S3
from diagrams.aws.database import Dynamodb
from diagrams.aws.iot import IotCore
from diagrams.onprem.client import Client

graph_attr = {
    "fontsize": "14",
    "bgcolor": "white",
    "pad": "0.5",
}

cluster_attr = {
    "fontsize": "12",
    "bgcolor": "#E8F4F8",
    "pencolor": "#232F3E",
    "margin": "20",
}

with Diagram(
    "IoT Noise Mapping Architecture",
    show=False,
    direction="LR",
    graph_attr=graph_attr,
    outformat="png"
):
    # Edge layer - IoT Device
    with Cluster("Edge Device", graph_attr=cluster_attr):
        device = IotCore("ESP32\nMEMS Mic\nWiFi")

    # AWS Cloud Services
    with Cluster("AWS Cloud", graph_attr=cluster_attr):
        # API Gateway for REST endpoint
        api = APIGateway("REST API")
        
        # Processing services
        with Cluster("Processing", graph_attr={"bgcolor": "#FFF5E6", "pencolor": "#232F3E"}):
            yamnet = ECS("YAMNet\nClassifier")
        
        # Storage layer
        with Cluster("Storage", graph_attr={"bgcolor": "#E6F3E6", "pencolor": "#232F3E"}):
            s3_bucket = S3("Audio\nRecordings")
            ddb = Dynamodb("Metadata &\nClassifications")

    # Flows with labels
    device >> Edge(label="POST /upload", color="#FF9900") >> api
    api >> Edge(label="invoke", color="#FF9900") >> yamnet
    yamnet >> Edge(label="store audio", color="#569A31") >> s3_bucket
    yamnet >> Edge(label="save metadata", color="#3B48CC") >> ddb
